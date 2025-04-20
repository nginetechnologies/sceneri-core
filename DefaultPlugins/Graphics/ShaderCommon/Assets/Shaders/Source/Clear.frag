#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
    layout(offset = 0) vec4 color;
	layout(offset = 16) float depth;
} pushConstants;

layout(location = 0) out vec4 outColor;
layout (depth_any) out float gl_FragDepth;

void main()
{
	outColor = pushConstants.color;
	gl_FragDepth = pushConstants.depth;
}
