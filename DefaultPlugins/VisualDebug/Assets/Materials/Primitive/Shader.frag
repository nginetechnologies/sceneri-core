#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_OES_standard_derivatives : enable

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
     layout(offset = 16) vec4 color;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor.rgba = pushConstants.color.rgba;
}
