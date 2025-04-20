#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 16) vec4 offsetAndRatio;
} pushConstants;

layout(set = baseSetIndex + 2, binding = 0) uniform texture2D sampledTexture;
layout(set = baseSetIndex + 2, binding = 1) uniform sampler textureSampler;

void main()
{
    outColor = texture(sampler2D(sampledTexture, textureSampler), pushConstants.offsetAndRatio.xy + fragTexCoord * pushConstants.offsetAndRatio.zw);
	if(outColor.a == 0)
	{
		discard;
	}
}
