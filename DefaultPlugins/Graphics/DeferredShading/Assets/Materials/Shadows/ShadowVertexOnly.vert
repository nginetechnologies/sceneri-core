#version 450

#define ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS

layout (location = 0) in vec3 inPosition;
layout(location = 1) in uint instanceIndex;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	uint shadowMatrixIndex;
} perViewData;

layout(std140, set = baseSetIndex, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = baseSetIndex, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;

layout (std140, set = baseSetIndex + 1, binding = 0) buffer readonly ShadowGenInfoBuffer
{
	layout(offset = 0) uint count;
	layout(offset = 16) mat4 viewProjMatrix[];
} shadowGenInfoBuffer;

void main()
{
	const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	mat4 viewProj = shadowGenInfoBuffer.viewProjMatrix[perViewData.shadowMatrixIndex];

	vec3 manipulatedPosition = perInstanceRotationMatrix * inPosition + perInstanceLocation;
	gl_Position = viewProj * vec4(manipulatedPosition, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
}
