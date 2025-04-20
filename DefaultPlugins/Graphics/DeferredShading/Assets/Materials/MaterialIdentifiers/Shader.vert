#version 450

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) mat4 view;
	layout(offset = 64) mat4 viewProjection;
} perViewData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint instanceIndex;

layout(std140, set = baseSetIndex + 0, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = baseSetIndex + 0, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;

void main()
{
	const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	vec3 manipulatedPosition = perInstanceRotationMatrix * inPosition + perInstanceLocation;
    gl_Position = perViewData.viewProjection * vec4(manipulatedPosition, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
}
