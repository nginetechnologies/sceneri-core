#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0) in vec2 inTexCoord;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) mat4 projectionMatrix;
	layout(offset = 64) mat4 inverseViewProjectionMatrix;
	layout(offset = 128) mat4 viewMatrix;
} pushConstants;

layout(set = baseSetIndex, binding = 0) uniform texture2D linearDepthTexture;
layout(set = baseSetIndex, binding = 1) uniform sampler linearDepthSampler;
layout(set = baseSetIndex, binding = 2) uniform texture2D normalTexture;
layout(set = baseSetIndex, binding = 3) uniform sampler normalSampler;
layout(set = baseSetIndex, binding = 4) uniform texture2D ssaoNoiseTexture;
layout(set = baseSetIndex, binding = 5) uniform sampler ssaoNoiseSampler;

const int SSAO_KERNEL_SIZE = 16;
layout(std140, set = baseSetIndex, binding = 6) buffer readonly KernelBuffer
{
	vec4 samples[SSAO_KERNEL_SIZE];
} kernelBuffer;

layout(location = 0) out vec4 outColor;

void main()
{
	const float SSAO_RADIUS = 0.5;

	const float sampledDepth = textureLod(sampler2D(linearDepthTexture, linearDepthSampler), inTexCoord, 0.0).r;
	if(sampledDepth <= 0.0)
	{
		outColor.a = 1.0;
		return;
	}

	const vec4 projectedFragPos = pushConstants.inverseViewProjectionMatrix * vec4(inTexCoord * 2.0 - 1.0, sampledDepth, 1.0);
	const vec3 fragPos = projectedFragPos.xyz / projectedFragPos.w;
	const vec3 fragPosViewSpace = vec3(pushConstants.viewMatrix * vec4(fragPos, 1.0));

	const vec3 sampledNormal = textureLod(sampler2D(normalTexture, normalSampler), inTexCoord, 0.0).rgb;
	const vec3 normal = normalize(sampledNormal * 2.0 - 1.0);

	// Get a random vector using a noise lookup
	ivec2 screenSize = textureSize(linearDepthTexture, 0);
	ivec2 noiseSize = textureSize(ssaoNoiseTexture, 0);
	const vec2 noiseUV = (vec2(screenSize) / vec2(noiseSize)) * inTexCoord;
	vec3 randomVec = texelFetch(sampler2D(ssaoNoiseTexture, ssaoNoiseSampler), ivec2(gl_FragCoord.xy) % noiseSize, 0).xyz * 2.0 - 1.0;

	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	const float bias = 0.025f;
	for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{
		vec3 samplePos = TBN * kernelBuffer.samples[i].xyz;
		samplePos = fragPosViewSpace.rgb + samplePos * SSAO_RADIUS;

		// project
		vec4 offset = vec4(samplePos, 1.0f);
		offset = pushConstants.projectionMatrix * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5f + 0.5f;

		const float offsetSampledDepth = textureLod(sampler2D(linearDepthTexture, linearDepthSampler), offset.xy, 0.0).r;
		const vec4 offsetProjectedFragPos = pushConstants.inverseViewProjectionMatrix * vec4(inTexCoord * 2.0 - 1.0, offsetSampledDepth, 1.0);
		const vec3 offsetFragPos = offsetProjectedFragPos.xyz / offsetProjectedFragPos.w;
		const vec3 adjustedSamplePosViewSpace = vec3(pushConstants.viewMatrix * vec4(offsetFragPos, 1.0));

		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPosViewSpace.z - adjustedSamplePosViewSpace.z));
		occlusion += (adjustedSamplePosViewSpace.z >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;
	}
	occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));

	outColor.a = occlusion * occlusion;
}
