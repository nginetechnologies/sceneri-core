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

layout(set = baseSetIndex + 2, binding = 0) uniform texture2D diffuseTexture;
layout(set = baseSetIndex + 2, binding = 1) uniform sampler diffuseSampler;
layout(set = baseSetIndex + 2, binding = 2) uniform texture2D normalTexture;
layout(set = baseSetIndex + 2, binding = 3) uniform sampler normalSampler;
layout(set = baseSetIndex + 2, binding = 4) uniform texture2D metallicTexture;
layout(set = baseSetIndex + 2, binding = 5) uniform sampler metallicSampler;
layout(set = baseSetIndex + 2, binding = 6) uniform texture2D roughnessTexture;
layout(set = baseSetIndex + 2, binding = 7) uniform sampler roughnessSampler;

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
	const vec4 albedo = texture(sampler2D(diffuseTexture, diffuseSampler), inTexCoord);
	if(albedo.a < 0.5)
	{
		discard;
	}

    outColor.xyz = srgbToLinear(albedo.rgb);
#if OUTPUT_DEPTH
	outDepth = gl_FragCoord.z;
#endif

	// Calculate normal in tangent space
	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent);
	vec3 B = normalize(inBitangent);

	B = cross(N, T);
	T = normalize(cross(B, N));
	mat3 TBN = mat3(T, B, N);
	const vec3 sampledNormal = texture(sampler2D(normalTexture, normalSampler), inTexCoord).xyz * 2.0 - vec3(1.0);
	vec3 tnorm = (TBN * normalize(sampledNormal));

	outNormal.xyz = tnorm*0.5+0.5;

	const float emissive = 0;
	const float metallic = texture(sampler2D(metallicTexture, metallicSampler), inTexCoord).r;
	const float roughness = texture(sampler2D(roughnessTexture, roughnessSampler), inTexCoord).r;
	outMaterialProperties.xyzw = vec4(metallic, roughness, emissive, 1.0);

	taaVelocity = CalcVelocity(inCurrentPosition, inPreviousPosition);
}
