#version 450

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) vec2 scale;
} pushConstants;

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

layout(std140, set = baseSetIndex, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in uint instanceIndex;

layout(std140, set = baseSetIndex + 1, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = baseSetIndex + 1, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;

layout(location = 0) out vec2 outTexCoord;

// TODO: Pass in scale modifier here to make the object as big as the text should be.

void main()
{
	const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	vec3 manipulatedPosition = perInstanceRotationMatrix * (inPosition * vec3(pushConstants.scale, 1)) + perInstanceLocation;

    gl_Position = viewInfo.current.viewProjectionMatrix * vec4(manipulatedPosition, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif

	outTexCoord = inTexCoord;
}
