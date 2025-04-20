#version 450

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) mat4 viewProjection;
	layout(offset = 64) vec4 colorAndLineLength;
} pushConstants;

layout (location = 0) in vec3 inPosition[];
layout (location = 1) in vec3 inNormal[];

layout (location = 0) out vec3 outColor;

void main()
{
	for(int i=0; i < gl_in.length(); i++)
	{
		vec3 pos = inPosition[i].xyz;
		vec3 normal = inNormal[i].xyz;

		gl_Position = pushConstants.viewProjection * vec4(pos, 1.0);
		outColor = pushConstants.colorAndLineLength.rgb;
		EmitVertex();

		gl_Position = pushConstants.viewProjection * vec4(pos + normal * pushConstants.colorAndLineLength.a, 1.0);
		outColor = pushConstants.colorAndLineLength.rgb;
		EmitVertex();

		EndPrimitive();
	}
}
