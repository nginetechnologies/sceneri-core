#version 450

#define ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS

#define LIGHT_COUNT 16

layout (triangles, invocations = LIGHT_COUNT) in;
layout (triangle_strip, max_vertices = 3) out;

layout (std140, set = 1, binding = 0) buffer readonly ShadowGenInfoBuffer
{
	layout(offset = 0) uint count;
	layout(offset = 16) mat4 viewProjMatrix[];
} shadowGenInfoBuffer;

void main()
{
	if(shadowGenInfoBuffer.count <= gl_InvocationID)
	{
		return;
	}

	for (int i = 0; i < gl_in.length(); i++)
	{
		gl_Layer = gl_InvocationID;
		gl_Position = shadowGenInfoBuffer.viewProjMatrix[gl_InvocationID] * gl_in[i].gl_Position;

		EmitVertex();
	}
	EndPrimitive();
}
