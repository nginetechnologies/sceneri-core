#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inViewDirection;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
    layout(offset = 0) vec4 color;
} pushConstants;

layout(location = 0) out vec4 outColor;

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

void main()
{
	const float exposure = 4.5;
	const float gamma = 2.2;

	// Tone mapping
	vec3 color = pushConstants.color.rgb;
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(gamma));

    outColor = vec4(color, pushConstants.color.a);
}
