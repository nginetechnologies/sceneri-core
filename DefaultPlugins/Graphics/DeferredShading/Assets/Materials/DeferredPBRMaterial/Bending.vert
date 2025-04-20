#version 450

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) vec4 dummy;
} pushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inCompressedNormal;
layout(location = 2) in uint inCompressedTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in uint instanceIndex;

layout(set = baseSetIndex + 2, binding = 0) uniform texture2D diffuseTexture;
layout(set = baseSetIndex + 2, binding = 1) uniform sampler diffuseSampler;

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
	mat3 invetrtedViewRotation;
	uvec4 renderAndOutputResolution;
	vec4 jitterOffset;
};

layout(std140, set = baseSetIndex + 0, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

layout(std140, set = baseSetIndex + 1, binding = 0) buffer readonly InstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} instanceRotationBuffer;
layout(std140, set = baseSetIndex + 1, binding = 1) buffer readonly InstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} instanceLocationBuffer;
layout(std140, set = baseSetIndex + 1, binding = 2) buffer readonly PrevInstanceRotationBuffer
{
	layout(offset = 0) mat3 rotations[];
} prevInstanceRotationBuffer;
layout(std140, set = baseSetIndex + 1, binding = 3) buffer readonly PrevInstanceLocationBuffer
{
	layout(offset = 0) vec3 locations[];
} prevInstanceLocationBuffer;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBitangent;
layout(location = 4) out vec4 outPreviousPosition;
layout(location = 5) out vec4 outCurrentPosition;

#define ENABLE_JITTER (!PLATFORM_WEB)

vec3 CalculateBentPosition(const float time)
{
	// TODO: Move these to constant values
	const float BendStrengh = 4.0f;
	const vec3 WindDirection = vec3(0.6,0.3,0.1);

	vec2 uv = vec2(inPosition.x + instanceIndex * 33, inPosition.y + instanceIndex * 12);
	float noise = texture(sampler2D(diffuseTexture, diffuseSampler), uv).r * 2;
	float bend = (BendStrengh / 100) * cos(time) * noise;
  	float rely = (inPosition.y * bend) + 1.0f;
  	float rely2 = rely * rely;
  	vec3 newre = ((rely2 * rely2) - rely2) * WindDirection;
  	return inPosition + vec3(newre.x, 0.0f, newre.y);
}

void main()
{
	const mat3 perInstanceRotationMatrix = instanceRotationBuffer.rotations[instanceIndex];
	const vec3 perInstanceLocation = instanceLocationBuffer.locations[instanceIndex];

	// TODO: Find a proper solution for meshes without uvs.
	
	vec3 currentBendPosition = CalculateBentPosition(viewInfo.current.viewLocationAndTime.w);
	vec3 manipulatedPosition = perInstanceRotationMatrix * currentBendPosition + perInstanceLocation;

    vec4 clipPosition = viewInfo.current.viewProjectionMatrix * vec4(manipulatedPosition, 1.0);

	// Calculate current frame jittered clipspace position
	vec4 clipPositionJittered = clipPosition + vec4(viewInfo.current.jitterOffset.xy * clipPosition.w, 0.0, 0.0);

	// Calculate previous frame clipspace position
	const mat3 prevPerInstanceRotationMatrix = prevInstanceRotationBuffer.rotations[instanceIndex];
	const vec3 prevPerInstanceLocation = prevInstanceLocationBuffer.locations[instanceIndex];
	vec3 previousBendPosition = CalculateBentPosition(viewInfo.previous.viewLocationAndTime.w);
	vec3 prevManipulatedPosition = prevPerInstanceRotationMatrix * previousBendPosition + prevPerInstanceLocation;
    vec4 previousClipPosition = viewInfo.previous.viewProjectionMatrix * vec4(prevManipulatedPosition, 1.0);

	vec4 previousClipPositionJittered = previousClipPosition + vec4(viewInfo.previous.jitterOffset.xy * clipPosition.w, 0.0, 0.0);

	outPreviousPosition = previousClipPositionJittered;
	outCurrentPosition = clipPositionJittered;

#if ENABLE_JITTER
	gl_Position = clipPositionJittered;
#else
	gl_Position = clipPosition;
#endif
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
	outTexCoord = inTexCoord;

	uvec3 compressedNormal = (uvec3(inCompressedNormal) >> uvec3(0, 10, 20)) & uvec3(1023);
	const float axisDivisor = 1.0 / 1023;
	const vec3 normal = vec3(-1.0) + vec3(compressedNormal * vec3(axisDivisor)) * vec3(2.0);

	uvec4 compressedTangent = (uvec4(inCompressedTangent) >> uvec4(0, 10, 20, 30)) & uvec4(1023, 1023, 1023, 3);
	const float signDivisor = 1.0 / 3.0;
	const vec4 tangent = vec4(-1.0) + vec4(compressedTangent * vec4(axisDivisor, axisDivisor, axisDivisor, signDivisor)) * vec4(2.0f);

	mat3 normalMatrix = perInstanceRotationMatrix;
	outNormal = normalize(normalMatrix * normalize(normal));
	outTangent = normalize(normalMatrix * normalize(tangent.xyz));
	outBitangent = cross(outNormal, outTangent) * tangent.w;
}
