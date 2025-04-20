#version 450

struct PerViewInfo
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 viewProjectionMatrixCylinderBillboard;
	mat4 viewProjectionMatrixSphericalBillboard;
	mat4 invertedViewProjectionMatrix;
	mat4 invertedViewMatrix;
	mat4 invertedProjectionMatrix;

	vec4 viewLocationAndTime;
	mat3 viewRotation;
	mat3 invertedViewRotation;
	uvec4 renderAndOutputResolution;
	vec4 jitterOffset;
};

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
layout(std140, set = baseSetIndex, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint instanceIndex;

layout(std140, set = baseSetIndex + 1, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = baseSetIndex + 1, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;

void main()
{
    const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	vec3 manipulatedPosition = perInstanceRotationMatrix * inPosition + perInstanceLocation;

    gl_Position = viewInfo.current.viewProjectionMatrix * vec4(manipulatedPosition, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
}
