#version 450

layout (location = 0) in vec3 inPosition;
layout(location = 1) in uint inCompressedTangent;
layout(location = 2) in uint instanceIndex;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) mat4 viewProjection;
} pushConstants;

layout(std140, set = baseSetIndex, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = baseSetIndex, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;

void main()
{
	const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	uvec4 compressedTangent = (uvec4(inCompressedTangent) >> uvec4(0, 10, 20, 30)) & uvec4(1023, 1023, 1023, 3);
	const float axisDivisor = 1.0 / 1023;
	const float signDivisor = 1.0 / 3.0;
	const vec4 tangent = vec4(-1.0) + vec4(compressedTangent * vec4(axisDivisor, axisDivisor, axisDivisor, signDivisor)) * vec4(2.0f);

	outNormal = perInstanceRotationMatrix * tangent.xyz;
	outPosition = perInstanceRotationMatrix * inPosition + perInstanceLocation;
	gl_Position = pushConstants.viewProjection * vec4(outPosition, 1.0);
}
