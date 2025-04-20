#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
    layout(offset = 128) uint materialIdentifier;
} pushConstants;

layout(location = 0) out uint outIdentifier;

void main()
{
	outIdentifier = pushConstants.materialIdentifier;
}
