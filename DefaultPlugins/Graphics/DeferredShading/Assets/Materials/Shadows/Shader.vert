#version 450

layout (location = 0) in vec3 inPosition;
layout(location = 1) in uint instanceIndex;

layout(std140, set = 0, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = 0, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;

void main()
{
	const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	vec3 manipulatedPosition = perInstanceRotationMatrix * inPosition + perInstanceLocation;
	gl_Position = vec4(manipulatedPosition, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
}
