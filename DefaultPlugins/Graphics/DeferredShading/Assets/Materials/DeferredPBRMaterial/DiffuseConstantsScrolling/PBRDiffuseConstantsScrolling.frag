#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBitangent;
layout(location = 4) in vec4 inPreviousPosition;
layout(location = 5) in vec4 inCurrentPosition;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;

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

layout(std140, set = baseSetIndex + 0, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

vec2 CalcVelocity(vec4 newPos, vec4 oldPos)
{
	newPos.xy -= (viewInfo.current.jitterOffset.xy * newPos.w);
	oldPos.xy -= (viewInfo.previous.jitterOffset.xy * oldPos.w);
	
	newPos.xy /= newPos.w;
	oldPos.xy /= oldPos.w;

	newPos.xy = (vec2(1.0) + newPos.xy) * vec2(0.5);
	oldPos.xy = (vec2(1.0) + oldPos.xy) * vec2(0.5);

	vec2 delta = newPos.xy - oldPos.xy;

	return delta.xy;
}

#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 16) float metallic;
	layout(offset = 20) float roughness;
	layout(offset = 24) vec2 speed;
} pushConstants;

layout(set = baseSetIndex + 2, binding = 0) uniform texture2D diffuseTexture;
layout(set = baseSetIndex + 2, binding = 1) uniform sampler diffuseSampler;

#if OUTPUT_DEPTH
layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outMaterialProperties;
layout(location = 4) out vec2 taaVelocity;
#else
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterialProperties;
layout(location = 3) out vec2 taaVelocity;
#endif

vec3 srgbToLinear(vec3 value)
{
	const vec3 less = step(vec3(0.04045), value);
	value = mix(value / vec3(12.92), pow((value + vec3(0.055)) / vec3(1.055), vec3(2.4)), less);
	return value;
}

void main()
{
	vec2 coordinates = inTexCoord + pushConstants.speed * viewInfo.current.viewLocationAndTime.w;

  	outColor.xyz = srgbToLinear(texture(sampler2D(diffuseTexture, diffuseSampler), coordinates).rgb);
#if OUTPUT_DEPTH
	outDepth = gl_FragCoord.z;
#endif

	outNormal.xyz = normalize(inNormal)*0.5+0.5;

	const float emissive = 0;
	outMaterialProperties.xyzw = vec4(pushConstants.metallic, pushConstants.roughness, emissive, 1.0);

	taaVelocity = CalcVelocity(inCurrentPosition, inPreviousPosition);
}
