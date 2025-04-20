#version 460

#define RAYTRACED_SHADOWS HAS_RAYTRACING
#define RAYTRACED_REFLECTIONS HAS_RAYTRACING

#if RAYTRACED_SHADOWS || RAYTRACED_REFLECTIONS
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#endif

#extension GL_EXT_samplerless_texture_functions : enable

#define ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS 1
//#define PSSM_CHOOSE_BEST_CASCADE

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

layout(std140, set = 1, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

layout(location = 0) out vec4 outColor;

#define SUBPASSES (RENDERER_VULKAN || RENDERER_METAL)
#define RASTERIZED_SHADOWS !RAYTRACED_SHADOWS

layout(binding = 0) uniform texture2D brdfTexture;
layout(binding = 1) uniform sampler brdfSampler;

// TODO: Change this to a cube array and make it clustered
layout(binding = 2) uniform utexture2D tilesTexture;
layout(binding = 3) uniform sampler tilesSampler;

#if SUBPASSES
layout(input_attachment_index = 0, binding = 4) uniform subpassInput inputAlbedo;
layout(input_attachment_index = 1, binding = 5) uniform subpassInput inputNormal;
layout(input_attachment_index = 2, binding = 6) uniform subpassInput inputMaterialProperties;
layout(input_attachment_index = 3, binding = 7) uniform subpassInput inputDepth;
const uint lastSubpassBindingIndex = 7;
#else
layout(binding = 4) uniform texture2D albedoTexture;
layout(binding = 5) uniform sampler albedoSampler;
layout(binding = 6) uniform texture2D normalTexture;
layout(binding = 7) uniform sampler normalSampler;
layout(binding = 8) uniform texture2D materialPropertiesTexture;
layout(binding = 9) uniform sampler materialPropertiesSampler;
layout(binding = 10) uniform texture2D depthTexture;
const uint lastSubpassBindingIndex = 10;
#endif

#if RAYTRACED_SHADOWS || RAYTRACED_REFLECTIONS
layout (binding = lastSubpassBindingIndex + 1, set = 0) uniform accelerationStructureEXT topLevelAccelerationStructure;
const uint lastShadowBindingIndex = lastSubpassBindingIndex + 1;
#elif RASTERIZED_SHADOWS
layout(binding = lastSubpassBindingIndex + 1) uniform texture2DArray shadowmapArrayTexture;
layout(binding = lastSubpassBindingIndex + 2) uniform sampler shadowmapSampler;
const uint lastShadowBindingIndex = lastSubpassBindingIndex + 2;
#endif

#define SUPPORT_CUBEMAP_ARRAYS 0

#if SUPPORT_CUBEMAP_ARRAYS
layout(binding = lastShadowBindingIndex + 1) uniform texture2DArray irradianceArrayTexture;
layout(binding = lastShadowBindingIndex + 2) uniform sampler irradianceSampler;
layout(binding = lastShadowBindingIndex + 3) uniform texture2DArray prefilteredMapArrayTexture;
layout(binding = lastShadowBindingIndex + 4) uniform sampler prefilteredMapSampler;
#else
layout(binding = lastShadowBindingIndex + 1) uniform textureCube irradianceTexture;
layout(binding = lastShadowBindingIndex + 2) uniform sampler irradianceSampler;
layout(binding = lastShadowBindingIndex + 3) uniform textureCube prefilteredMapTexture;
layout(binding = lastShadowBindingIndex + 4) uniform sampler prefilteredMapSampler;
#endif

const uint lastCubemapBindingIndex = lastShadowBindingIndex + 4; // 12

struct PointLight
{
	vec4 positionAndInfluenceRadius;
	vec4 colorAndInverseSquareInfluenceRadius;
	int shadowMapIndex;
	mat4 viewMatrices[6];
};

struct SpotLight
{
	vec4 positionAndInfluenceRadius;
	vec4 colorAndInverseSquareInfluenceRadius;
	int shadowMapIndex;
	mat4 viewMatrix;
};

#define MAXIMUM_CASCADES 4

struct DirectionalLight
{
#if !ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
	vec4 cascadeSplits;
#endif
	vec3 direction;
	uint cascadeCount;
	vec3 color;
	int shadowMapIndex;
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
	uint shadowMatrixIndex;
#else
	mat4 viewMatrices[MAXIMUM_CASCADES];
#endif
};

layout(std140, binding = lastCubemapBindingIndex + 1) buffer readonly LightBuffer
{
	layout(offset = 0) PointLight lights[];
} pointLightBuffer;

layout(std140, binding = lastCubemapBindingIndex + 2) buffer readonly SpotLightBuffer
{
	layout(offset = 0) SpotLight lights[];
} spotLightBuffer;

layout(std140, binding = lastCubemapBindingIndex + 3) buffer readonly DirectionalLightBuffer
{
#if RENDERER_WEBGPU
	uint count;
#endif
	DirectionalLight lights[];
} directionalLightBuffer;

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS && RASTERIZED_SHADOWS
	layout(std430) struct SDSMShadowSamplingInfo
	{
		mat4 ViewProjMatrix[MAXIMUM_CASCADES];
		vec4 CascadeSplits;
	};

	layout(std430, binding = lastCubemapBindingIndex + 4) buffer readonly SDSMShadowSamplingInfoBuffer
	{
		layout(offset = 0) SDSMShadowSamplingInfo lights[];
	} sdsmShadowSamplingInfoBuffer;
#elif RAYTRACED_REFLECTIONS
struct RenderItemInstance
{
	uint meshIndex;
    //uint materialIndex;
    int diffuseTextureId;
    int roughnessTextureId;
};

struct Mesh
{
    uint64_t vertexNormalsBufferAddress;
    uint64_t vertexTextureCoordinatesBufferAddress;
    uint64_t indicesBufferAddress;
};

struct MaterialInstance
{
    vec3 diffuseColor;
    int diffuseTextureId;
    float roughness;
    int roughnessTextureId;
};

layout(std140, binding = lastShadowBindingIndex + 8, set = 0) readonly buffer RenderItemsBuffer { RenderItemInstance[] renderItems; } renderItemsBuffer;
//layout(binding = 18, set = 0) readonly buffer MaterialInstancesBuffer { MaterialInstance[] materialInstances; } materialInstancesBuffer;

#define MAXIMUM_TEXTURE_COUNT 65535 // Corresponds to TextureIdentifier in C++
layout(binding = 0, set = 2) uniform texture2D[MAXIMUM_TEXTURE_COUNT] textures;
layout(binding = 1, set = 2) uniform sampler[MAXIMUM_TEXTURE_COUNT] textureSamplers;

#define MAXIMUM_MESH_COUNT 65535 // Corresponds to StaticMeshIdentifier in C++
layout(std140, binding = 0, set = 3) buffer readonly MeshesBuffer
{
	layout(offset = 0) Mesh meshes[MAXIMUM_MESH_COUNT];
} meshesBuffer;
#endif

#define PI 3.1415926535897932384626433832795

#define USE_PCF (!RENDERER_WEBGPU)

uint getRayCubefaceIndex(const vec3 rayDirection)
{
	const vec3 direction = abs(rayDirection);

	const uvec3 greater = uvec3(greaterThan(
		direction.xyx,
		direction.yzz
	));
	const uvec3 lesserOrEqual = uvec3(lessThanEqual(
		direction.xyx,
		direction.yzz
	));
	const uvec3 positiveAxes = uvec3(equal(
		rayDirection,
		direction
	));

	return greater.x * greater.z * positiveAxes.x +
		lesserOrEqual.x * greater.y * (positiveAxes.y + 2) +
		lesserOrEqual.z * lesserOrEqual.y * (positiveAxes.z + 4);
}

vec3 sgFace2DMapping[6][2] =
{
    //XPOS face
    {
		vec3( 0,  0, -1),   //u towards negative Z
		vec3( 0, -1,  0)   //v towards negative Y
	},
    //XNEG face
     {
	  vec3(0,  0,  1),   //u towards positive Z
      vec3(0, -1,  0)   //v towards negative Y
	 },
    //YPOS face
    {
		vec3(1, 0, 0),     //u towards positive X
		vec3(0, 0, 1)     //v towards positive Z
	},
    //YNEG face
    {
	vec3(1, 0, 0),     //u towards positive X
     vec3(0, 0 , -1)   //v towards negative Z
	},
    //ZPOS face
    {
	vec3(1, 0, 0),     //u towards positive X
     vec3(0, -1, 0)    //v towards negative Y
	},
    //ZNEG face
    {
	vec3(-1, 0, 0),    //u towards negative X
     vec3(0, -1, 0)    //v towards negative Y
	}
};

vec2 getRayCubefaceCoordinatesAndIndex(vec3 rayDirection, out uint faceIndex)
{
	const vec3 direction = abs(rayDirection);
	float maxCoord;

	if(direction.x >= direction.y && direction.x >= direction.z)
	{
		maxCoord = direction.x;

		faceIndex = rayDirection.x >= 0 ? 0 : 1;
	}
	else if(direction.y >= direction.x && direction.y >= direction.z)
	{
		maxCoord = direction.y;

		faceIndex = rayDirection.y >= 0 ? 2 : 3;
	}
	else
	{
		maxCoord = direction.z;

		faceIndex = rayDirection.z >= 0 ? 4 : 5;
	}

	const vec3 onFaceDir = rayDirection * (1.f / abs(maxCoord));
	float nvcU = (dot(sgFace2DMapping[faceIndex][0], onFaceDir) + 1.0f) / 2;
	float nvcY = (dot(sgFace2DMapping[faceIndex][1], onFaceDir) + 1.0f) / 2;

	return vec2(nvcU, nvcY);
}

#if RASTERIZED_SHADOWS
float textureProj(vec4 shadowCoord, const float layer, const vec2 offset)
{
	const vec2 sampledCoordinate = shadowCoord.st + offset;

	return texture(sampler2DArrayShadow(shadowmapArrayTexture, shadowmapSampler), vec4(sampledCoordinate, layer, shadowCoord.z)).r;
}

float filterPCF(const vec4 sc, const float layer)
{
	const ivec2 texDim = textureSize(sampler2DArrayShadow(shadowmapArrayTexture, shadowmapSampler), 0).xy;
	const float scale = 1.0;
	const vec2 delta = scale * vec2(1.0) / vec2(texDim.xy);

	float shadowFactor = 0.0;
	const int range = 1;
	const int rangeRowCount = range * 2 + 1;
		const int count = rangeRowCount * rangeRowCount;
	const float inverseCount = 1.0 / float(count);

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, layer, delta * vec2(x, y));
		}

	}
	return shadowFactor * inverseCount;
}

float shadow(const vec4 shadowClip, const uint i)
{
	#if USE_PCF
		return filterPCF(shadowClip, i);
	#else
		return textureProj(shadowClip, i, vec2(0.0));
	#endif
}
#endif

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(const vec3 diffuseColor)
{
	return diffuseColor / PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(const vec3 reflectance0, const vec3 reflectance90, const float VdotH)
{
	return reflectance0 + (reflectance90 - reflectance0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(const float NdotL, const float NdotV, const float roughnessSquared)
{
	const vec2 NdotX = vec2(NdotL, NdotV);
	const vec2 attenuation = vec2(2.0) * NdotX / (NdotX + sqrt(vec2(roughnessSquared) + (vec2(1.0 - roughnessSquared)) * (NdotX * NdotX)));
	return attenuation.x * attenuation.y;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(const float roughnessSquared, const float NdotH)
{
	const float f = (NdotH * roughnessSquared - NdotH) * NdotH + 1.0;
	return roughnessSquared / (PI * f * f);
}

vec3 applyLight(vec3 n, vec3 v, float NdotV, vec3 l, vec3 specularEnvironmentR0, vec3 specularEnvironmentR90, float alphaRoughness, vec3 diffuseColor, vec3 color, float attenuation)
{
	const vec3 h = normalize(l+v);

	const float NdotL = clamp(dot(n, l), 0.001, 1.0);
	const float NdotH = clamp(dot(n, h), 0.0, 1.0);
	const float VdotH = clamp(dot(v, h), 0.0, 1.0);

	// Calculate the shading terms for the microfacet specular shading model
	const vec3 F = specularReflection(specularEnvironmentR0, specularEnvironmentR90, VdotH);

	const float G = geometricOcclusion(NdotL, NdotV, alphaRoughness);
	const float D = microfacetDistribution(alphaRoughness, NdotH);

	// Calculation of analytical lighting contribution
	const vec3 diffuseContrib = (1.0 - F) * diffuse(diffuseColor);
	const vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	return NdotL * color * attenuation * (diffuseContrib + specContrib);
}

vec3 F_SchlickR(const float cosTheta, const vec3 F0, const float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(const vec3 R, const float roughness, uint arrayIndex)
{
#if SUPPORT_CUBEMAP_ARRAYS
	const float MAX_REFLECTION_LOD = textureQueryLevels(samplerCube(prefilteredMapTexture, prefilteredMapSampler));
#else
	const float MAX_REFLECTION_LOD = textureQueryLevels(samplerCube(prefilteredMapTexture, prefilteredMapSampler));
#endif

	float lod = roughness * MAX_REFLECTION_LOD;
	const float lodf = floor(lod);
	const float lodc = ceil(lod);

#if SUPPORT_CUBEMAP_ARRAYS
	const vec3 a = textureLod(samplerCubeArray(prefilteredMapArray, prefilteredMapSampler), vec4(R, arrayIndex), lodf).rgb;
	const vec3 b = textureLod(samplerCubeArray(prefilteredMapArray, prefilteredMapSampler), vec4(R, arrayIndex), lodc).rgb;
#else
	const vec3 a = textureLod(samplerCube(prefilteredMapTexture, prefilteredMapSampler), R, lodf).rgb;
	const vec3 b = textureLod(samplerCube(prefilteredMapTexture, prefilteredMapSampler), R, lodc).rgb;
#endif

	return mix(a, b, lod - lodf);
}


//cutoff window is from frostbite https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf

float LightDistanceAttenuationCutoffWindow( float sqrLightDistance, float invSqrLightInfluenceRadius )
{
  float factor = sqrLightDistance * invSqrLightInfluenceRadius;
  float fac = clamp(1.0 - factor * factor, 0.0, 1.0);
  return fac * fac;
}


//based on https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/

float LightDistanceAttenuation( float lightDistance, float lightInfluenceRadius, float invSqrLightInfluenceRadius )
{
	const float lightRadius = 0.25; //This is the light source radius, not the influence radius
	float d = max( lightDistance - lightRadius, 0.0 );

	float denom = 1.0 + d / lightRadius;
	float attenuation = 1.0 / (denom*denom);
	attenuation *= LightDistanceAttenuationCutoffWindow( lightDistance * lightDistance, invSqrLightInfluenceRadius ); //Just to make sure the attenuation smoothly drops to zero at light radius

	return attenuation;
}

vec3 ScreenSpaceDither(vec2 vScreenPos) //TODO this is just a quick&dirty dither, improve this
{
    vec3 vDither = vec3(dot(vec2(131.0, 312.0), vScreenPos.xy ));
    vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0)) - vec3(0.5, 0.5, 0.5);
    return (vDither.rgb / 255.0);
}

/*
float Remap( float v, float mini, float maxi )
{
	return (v - mini)/(maxi - mini);
}

void DbgShadowMap( inout vec3 color, vec2 screenUV, int i, uint mapIndex )
{
	const float boxSize = 0.1;
	const float spacing = 0.03;

	const float startX = spacing + (boxSize + spacing) * i;

	vec2 uv = vec2( Remap(screenUV.x, startX, startX + boxSize ),
					Remap(screenUV.y, spacing, spacing + boxSize * 1.5 ) );

	if( (uv.x < 0.0) || (uv.y < 0.0) || (uv.x > 1.0) || (uv.y > 1.0) )
		return;

	color = texture(sampler2DArrayShadow(shadowmapArrayTexture, shadowmapSampler), vec3(uv, mapIndex)).rrr;
}
*/

#if RAYTRACED_REFLECTIONS
struct RayHitMaterialInfo
{
	vec4 albedoAndDistance;
	vec3 scatterDirection;
};

vec2 Mix(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 Mix(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 RandomInUnitSphere(const uint seed)
{
	return vec3(0); // todo
}

layout(buffer_reference, scalar) buffer VertexNormals {vec4 normals[]; };
layout(buffer_reference, scalar) buffer VertexTextureCoordinates {vec2 textureCoordinates[]; };
layout(buffer_reference, scalar) buffer Indices {uint indices[]; };

RayHitMaterialInfo GetRayHitMaterialInfo(const uint renderItemIndex, const uint triangleIndex, const vec2 barycentrics2, const float distance, const vec3 rayOrigin, const vec3 rayDirection, const uint seed)
{
	 const vec3 barycentrics = vec3(1.0 - barycentrics2.x - barycentrics2.y, barycentrics2.x, barycentrics2.y);

	 const RenderItemInstance renderItem = renderItemsBuffer.renderItems[renderItemIndex];
	 const Mesh mesh = meshesBuffer.meshes[renderItem.meshIndex];

	 Indices indices = Indices(mesh.indicesBufferAddress);
	 VertexNormals vertexNormals = VertexNormals(mesh.vertexNormalsBufferAddress);
	 VertexTextureCoordinates vertexTextureCoordinates = VertexTextureCoordinates(mesh.vertexTextureCoordinatesBufferAddress);

	 const uint baseVertexIndex = triangleIndex * 3;
	 uvec3 vertexIndices = uvec3(indices.indices[baseVertexIndex + 0], indices.indices[baseVertexIndex + 1], indices.indices[baseVertexIndex + 2]);

	 vec2 meshTextureCoordinates = Mix(vertexTextureCoordinates.textureCoordinates[vertexIndices[0]], vertexTextureCoordinates.textureCoordinates[vertexIndices[1]], vertexTextureCoordinates.textureCoordinates[vertexIndices[2]], barycentrics);
	 vec3 meshNormal = Mix(vertexNormals.normals[vertexIndices[0]].rgb, vertexNormals.normals[vertexIndices[1]].rgb, vertexNormals.normals[vertexIndices[2]].rgb, barycentrics);

	 /*const vec3 hitLocation = rayOrigin + rayDirection * distance;

	 //const MaterialInstance materialInstance = materialInstancesBuffer.materialInstances[renderItem.materialIndex];
	 *///const vec3 albedo = materialInstance.diffuseColor + ((materialInstance.diffuseTextureId != -1) ? texture(sampler2D(textures[nonuniformEXT(materialInstance.diffuseTextureId)], textureSamplers[nonuniformEXT(materialInstance.diffuseTextureId)]), meshTextureCoordinates).rgb : vec3(0, 0, 0));
	 
	const vec3 albedo = ((renderItem.diffuseTextureId != -1) 
    ? texture(sampler2D(textures[nonuniformEXT(renderItem.diffuseTextureId)], 
                        textureSamplers[nonuniformEXT(renderItem.diffuseTextureId)]), 
              meshTextureCoordinates).rgb 
    : vec3(0, 0, 0));

	 //const vec3 albedo = vec3(renderItemIndex, renderItem.meshIndex, renderItem.diffuseTextureId);
	 //const vec3 meshNormal = vec3(0, 0, 1);
	// vec3 albedo = vec3(renderItemIndex, renderItem.meshIndex, 0);

	 const vec3 reflected = reflect(rayDirection, meshNormal);
	 const bool isScattered = dot(reflected, meshNormal) > 0;

	 const vec3 scatterDirection = reflected;// + materialInstance.roughness * RandomInUnitSphere(seed);

	 return RayHitMaterialInfo
	 (
		vec4(albedo, isScattered ? distance : -1.0f),
		scatterDirection
	 );
}
#endif

void main()
{
	const vec2 texSize = viewInfo.current.renderAndOutputResolution.xy;
	const vec2 inTexCoord = gl_FragCoord.xy / texSize;

#if SUBPASSES
	const float sampledDepth = subpassLoad(inputDepth).r;
#else
	const float sampledDepth = texelFetch(depthTexture, ivec2(gl_FragCoord.xy), 0).r;
#endif
#if !RENDERER_WEBGPU
	if(sampledDepth <= 0.0)
	{
		outColor = vec4(0, 0, 0, 0);
		return;
	}
#endif

	const vec4 projectedFragPos = viewInfo.current.invertedViewProjectionMatrix * vec4(inTexCoord * 2.0 - 1.0, sampledDepth, 1.0);
	const vec3 fragPos = projectedFragPos.xyz / projectedFragPos.w;

#if SUBPASSES
	const vec3 sampledAlbedo = subpassLoad(inputAlbedo).rgb;
	const vec3 sampledNormal = subpassLoad(inputNormal).rgb;
	const vec4 materialProperties = subpassLoad(inputMaterialProperties).rgba;
#else
	const vec3 sampledAlbedo = texture(sampler2D(albedoTexture, albedoSampler), inTexCoord).rgb;
	const vec3 sampledNormal = texture(sampler2D(normalTexture, normalSampler), inTexCoord).rgb;
	const vec4 materialProperties = texture(sampler2D(materialPropertiesTexture, materialPropertiesSampler), inTexCoord).rgba;
#endif
	const vec3 albedo = sampledAlbedo;
	const float metallic = materialProperties.r;

	const vec3 normal = normalize((sampledNormal.rgb * 2.0 - 1.0));

	const float minimumRoughness = 0.04;
	const float roughness = clamp(materialProperties.g, minimumRoughness, 1.0);
	const float alphaRoughness = roughness * roughness;

	const float emissive = materialProperties.b;

	const vec3 V = normalize(viewInfo.current.viewLocationAndTime.xyz - fragPos);
	const float NdotV = clamp(abs(dot(normal, V)), 0.001, 1.0);

	const vec3 F0 = vec3(0.04);
	const vec3 specularColor = mix(F0, albedo.rgb, metallic);
	const vec3 diffuseColor = albedo * (vec3(1.0) - F0) * (1.0 - metallic);

	const float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	const float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	const vec3 specularEnvironmentR0 = specularColor.rgb;
	const vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	outColor.rgb = vec3(0, 0, 0);

	// TODO: Change to cluster
	// Just need to figure out depth index
	const ivec2 clusterTexCoord = ivec2(inTexCoord * textureSize(usampler2D(tilesTexture, tilesSampler), 0));
	const uvec4 clusterData = texelFetch(usampler2D(tilesTexture, tilesSampler), clusterTexCoord, 0);

	float globalShadowMult = 1.0;

	const uint pointLightRange = clusterData.r;
	const uint firstPointLightIndex = pointLightRange & 0xFFFF;
	const uint endPointLightIndex = pointLightRange >> 16;
	for(uint lightIndex = firstPointLightIndex; lightIndex != endPointLightIndex; ++lightIndex)
	{
		vec3 l = pointLightBuffer.lights[lightIndex].positionAndInfluenceRadius.xyz - fragPos.rgb;

		// Distance from light to fragment position
		const float dist = length(l);

		// Light to fragment
		const vec3 lightDirection = l / dist;

		if(dot(normal, lightDirection) < 0)
		{
			continue;
		}

		const float influenceRadius = pointLightBuffer.lights[lightIndex].positionAndInfluenceRadius.w;
		float attenuation = LightDistanceAttenuation( dist,
			influenceRadius, pointLightBuffer.lights[lightIndex].colorAndInverseSquareInfluenceRadius.w );

		if(attenuation > 0.0)
		{
			vec3 lightColor = applyLight(normal, V, NdotV, lightDirection, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness, diffuseColor, pointLightBuffer.lights[lightIndex].colorAndInverseSquareInfluenceRadius.xyz, attenuation);

#if RAYTRACED_SHADOWS
			rayQueryEXT rayQuery;
			rayQueryInitializeEXT(rayQuery, topLevelAccelerationStructure, gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos, 0.01, lightDirection, min(dist, influenceRadius * 2));

			// Traverse the acceleration structure and store information about the first intersection (if any)
			rayQueryProceedEXT(rayQuery);

			if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
			{
				// This fragment is in shadow
				lightColor *= 0.1;
			}
#elif RASTERIZED_SHADOWS && !RENDERER_WEBGPU
			if(pointLightBuffer.lights[lightIndex].shadowMapIndex >= 0)
			{
				const uint cubefaceIndex = getRayCubefaceIndex(l);
				const vec4 clip = (pointLightBuffer.lights[lightIndex].viewMatrices[cubefaceIndex]) * vec4(fragPos.rgb, 1.0);
				const vec4 coordinate = clip / clip.w;

				const float shadowMult = shadow(coordinate, pointLightBuffer.lights[lightIndex].shadowMapIndex + cubefaceIndex);
				lightColor *= shadowMult;
				globalShadowMult *= shadowMult;
			}
#endif

			outColor.rgb += lightColor;
		}
	}
	
	const uint spotLightRange = clusterData.g;
	const uint firstSpotLightIndex = spotLightRange & 0xFFFF;
	const uint endSpotLightIndex = spotLightRange >> 16;
	for(uint lightIndex = firstSpotLightIndex; lightIndex != endSpotLightIndex; ++lightIndex)
	{
		const vec4 clip = spotLightBuffer.lights[lightIndex].viewMatrix * vec4(fragPos.rgb, 1.0);
		const vec4 coordinate = clip / clip.w;

		if(coordinate.w >= 0 && min(coordinate.x, coordinate.y) >= 0 && max(coordinate.x, coordinate.y) <= 1.0)
		{
			vec3 l = spotLightBuffer.lights[lightIndex].positionAndInfluenceRadius.xyz - fragPos.rgb;
			// Distance from light to fragment position
			const float dist = length(l);
			// Light to fragment
			l = l / dist;

			if(dot(normal, l) < 0)
			{
				continue;
			}

			float attenuation = LightDistanceAttenuation( dist,
				spotLightBuffer.lights[lightIndex].positionAndInfluenceRadius.w, spotLightBuffer.lights[lightIndex].colorAndInverseSquareInfluenceRadius.w );

			if(attenuation > 0.0)
			{
				vec3 lightColor = applyLight(normal, V, NdotV, l, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness, diffuseColor, spotLightBuffer.lights[lightIndex].colorAndInverseSquareInfluenceRadius.xyz, attenuation);

#if RAYTRACED_SHADOWS
				rayQueryEXT rayQuery;
				rayQueryInitializeEXT(rayQuery, topLevelAccelerationStructure, gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos, 0.01, l, 1000.0);

				// Traverse the acceleration structure and store information about the first intersection (if any)
				rayQueryProceedEXT(rayQuery);

				if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
				{
					// This fragment is in shadow
					lightColor *= 0.1;
				}
#elif RASTERIZED_SHADOWS && !RENDERER_WEBGPU
				if(spotLightBuffer.lights[lightIndex].shadowMapIndex >= 0)
				{
					const float shadowMult = shadow(coordinate, spotLightBuffer.lights[lightIndex].shadowMapIndex);
					lightColor *= shadowMult;
					globalShadowMult *= shadowMult;
				}
#endif

				outColor.rgb += lightColor;
			}
		}
	}

	const uint directionalLightRange = clusterData.b;
	const uint firstDirectionalLightIndex = directionalLightRange & 0xFFFF;
	const uint endDirectionalLightIndex = directionalLightRange >> 16;
	vec3 directionalLightColor = vec3(0, 0, 0);
	for(uint lightIndex = firstDirectionalLightIndex; lightIndex != endDirectionalLightIndex; ++lightIndex)
	{
		const vec3 l = directionalLightBuffer.lights[lightIndex].direction;
		if(dot(normal, l) < 0)
		{
			continue;
		}

		vec3 lightColor = applyLight(normal, V, NdotV, l, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness, diffuseColor, directionalLightBuffer.lights[lightIndex].color, 1.f);

#if RAYTRACED_SHADOWS
		rayQueryEXT rayQuery;
		rayQueryInitializeEXT(rayQuery, topLevelAccelerationStructure, gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, fragPos, 0.01, l, 1000.0);

		// Traverse the acceleration structure and store information about the first intersection (if any)
		rayQueryProceedEXT(rayQuery);

		if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
		{
			// This fragment is in shadow
			lightColor *= 0.1;
		}
#elif RASTERIZED_SHADOWS && !RENDERER_WEBGPU
		if(directionalLightBuffer.lights[lightIndex].shadowMapIndex >= 0)
		{
			uint numCascades = directionalLightBuffer.lights[lightIndex].cascadeCount;
			uint selectedCascadeIndex = numCascades - 1;

			const vec3 localFrag = viewInfo.current.invertedViewRotation * (fragPos.rgb - viewInfo.current.viewLocationAndTime.xyz);
			vec4 coordinate;

			//TODO handle z > lastCascadeSplit

			for(uint cascadeIndex = 0; cascadeIndex < numCascades - 1; ++cascadeIndex)
			{
			#if PSSM_CHOOSE_BEST_CASCADE
				#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
					uint shadowMatrixIndex = directionalLightBuffer.lights[lightIndex].shadowMatrixIndex + cascadeIndex;
					const mat4 shadowMatrix = sdsmShadowSamplingInfoBuffer.lights[lightIndex].ViewProjMatrix[shadowMatrixIndex];
				#else
					const mat4 shadowMatrix = directionalLightBuffer.lights[lightIndex].viewMatrices[cascadeIndex];
				#endif

				const vec4 clip = shadowMatrix * vec4(fragPos.rgb, 1.0);
				coordinate = clip / clip.w;

				const float cascadeMargin = 0.01;

				if( 	all( greaterThan( coordinate.xy, vec2( cascadeMargin, cascadeMargin ) ) ) &&
					all( lessThan( coordinate.xy, vec2( 1.0 - cascadeMargin, 1.0 - cascadeMargin ) ) ) )
				{
					selectedCascadeIndex = cascadeIndex;
					break;
				}
			#else
				#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
					float cascadeSplit = sdsmShadowSamplingInfoBuffer.lights[lightIndex].CascadeSplits[cascadeIndex];
				#else
					float cascadeSplit = directionalLightBuffer.lights[lightIndex].cascadeSplits[cascadeIndex];
				#endif

				if(localFrag.y < cascadeSplit)
				{
					selectedCascadeIndex = cascadeIndex;
					break;
				}
			#endif
			}

			#if DEBUG_CASCADES
			switch(selectedCascadeIndex)
			{
				case 0: lightColor = vec3(1, 0, 0);
				break;
				case 1: lightColor = vec3(0, 1, 0);
				break;
				case 2: lightColor = vec3(0, 0, 1);
				break;
				case 3: lightColor = vec3(1, 1, 0);
				break;
			}
			#endif

			#if !PSSM_CHOOSE_BEST_CASCADE
				#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
					uint shadowMatrixIndex = directionalLightBuffer.lights[lightIndex].shadowMatrixIndex + selectedCascadeIndex;
					const mat4 shadowMatrix = sdsmShadowSamplingInfoBuffer.lights[lightIndex].ViewProjMatrix[shadowMatrixIndex];
				#else
					const mat4 shadowMatrix = directionalLightBuffer.lights[lightIndex].viewMatrices[selectedCascadeIndex];
				#endif

				const vec4 clip = shadowMatrix * vec4(fragPos.rgb, 1.0);
				coordinate = clip / clip.w;
			#endif

			const float shadowMult = shadow(coordinate, directionalLightBuffer.lights[lightIndex].shadowMapIndex + selectedCascadeIndex);
			lightColor *= shadowMult;
			globalShadowMult *= shadowMult;
		}
#endif

		directionalLightColor += lightColor;
	}

#if RENDERER_WEBGPU
	if(directionalLightBuffer.count > 0)
	{
		const uint lightIndex = 0;
		if(directionalLightBuffer.lights[lightIndex].shadowMapIndex >= 0)
		{
			uint numCascades = directionalLightBuffer.lights[lightIndex].cascadeCount;
			uint selectedCascadeIndex = numCascades - 1;

			const vec3 localFrag = viewInfo.current.invertedViewRotation * (fragPos.rgb - viewInfo.current.viewLocationAndTime.xyz);
			vec4 coordinate;

			//TODO handle z > lastCascadeSplit

			for(uint cascadeIndex = 0; cascadeIndex < numCascades - 1; ++cascadeIndex)
			{
				float cascadeSplit = sdsmShadowSamplingInfoBuffer.lights[lightIndex].CascadeSplits[cascadeIndex];
				if(localFrag.y < cascadeSplit)
				{
					selectedCascadeIndex = cascadeIndex;
					break;
				}
			}

			uint shadowMatrixIndex = directionalLightBuffer.lights[lightIndex].shadowMatrixIndex + selectedCascadeIndex;
			const mat4 shadowMatrix = sdsmShadowSamplingInfoBuffer.lights[lightIndex].ViewProjMatrix[shadowMatrixIndex];

			const vec4 clip = shadowMatrix * vec4(fragPos.rgb, 1.0);
			coordinate = clip / clip.w;

			const float shadowMult = shadow(coordinate, directionalLightBuffer.lights[lightIndex].shadowMapIndex + selectedCascadeIndex);
			directionalLightColor *= shadowMult;
			globalShadowMult *= shadowMult;
		}
	}
#endif

	outColor.rgb += directionalLightColor;

	const vec3 R = normalize(reflect(-V, normal));

	mat3 rot = mat3(vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, 1, 0));

	const uint environmentIndex = uint(clusterData.a);

	const vec2 brdf = texture(sampler2D(brdfTexture, brdfSampler), vec2(max(NdotV, 0.0), 1.0 - roughness)).rg;

	vec3 reflection = prefilteredReflection(rot * R, roughness, environmentIndex).rgb;

	vec3 irradiance;
	{
		const vec3 RNormal = rot * normal;

#if SUPPORT_CUBEMAP_ARRAYS
		irradiance = texture(samplerCubeArray(irradianceArrayTexture, irradianceSampler), vec4(RNormal, environmentIndex)).rgb;
#else
		irradiance = texture(samplerCube(irradianceTexture, irradianceSampler), RNormal).rgb;
#endif
	}

	// Diffuse based on irradiance
	const vec3 diffuse = irradiance * diffuseColor;

	const vec3 F = F_SchlickR(max(NdotV, 0.0), specularColor, roughness);

#if RAYTRACED_REFLECTIONS
	const float maxReflectionRoughness = 0.4;
	if(roughness <= maxReflectionRoughness)
	{
		vec3 rayOrigin = fragPos;
		vec3 rayDirection = R;

		reflection = vec3(1, 1, 1);

		const uint maximumBounceCount = 1;
		for(uint bounceIndex = 0; bounceIndex < maximumBounceCount; bounceIndex++)
		{
			rayQueryEXT rayQuery;
			rayQueryInitializeEXT(rayQuery, topLevelAccelerationStructure, gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, rayOrigin + rayDirection * 0.01, 0.05, rayDirection, 10000.0);

			// Traverse the acceleration structure and store information about the first intersection (if any)
			rayQueryProceedEXT(rayQuery);

			if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
			{
				const int renderItemIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
				const int triangleIndex = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
				const vec2 barycentrics2 = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
				const float distance = rayQueryGetIntersectionTEXT(rayQuery, true);
				const uint seed = 0;

				const RayHitMaterialInfo rayHitMaterialInfo = GetRayHitMaterialInfo(renderItemIndex, triangleIndex, barycentrics2, distance, rayOrigin, rayDirection, seed);

				vec3 color = rayHitMaterialInfo.albedoAndDistance.xyz;
				reflection *= color;

				if(rayHitMaterialInfo.albedoAndDistance.w < 0)
				{
					break;
				}

				rayOrigin += rayDirection * rayHitMaterialInfo.albedoAndDistance.w;
				rayDirection = rayHitMaterialInfo.scatterDirection.xyz;
			}
		}
	}
#endif

	// IBL
	const vec3 specularReflectance = reflection * (specularColor * brdf.x + brdf.y);
	const vec3 kD = (1.0 - F) * (1.0 - metallic);
	outColor.rgb += kD * diffuse + specularReflectance;

	outColor.rgb += albedo * (emissive * 10000.0f);

#if RENDERER_WEBGPU
	// Tone mapping because of the missing post processing stage on web
	// outColor.rgb = FilmicTonemap(hdrScene.rgb * exposure);
	const vec3 FilmicTonemap = max(vec3(0.0), outColor.rgb - 0.004);
	outColor.rgb = (FilmicTonemap * (6.2 * FilmicTonemap + 0.5)) / (FilmicTonemap * (6.2 * FilmicTonemap + 1.7) + 0.06);
	outColor.rgb = pow(outColor.rgb, vec3(2.2));
	// outColor.rgb += ScreenSpaceDither(gl_GlobalInvocationID.xy);
	vec3 vDither = vec3(dot(vec2(131.0, 312.0), gl_FragCoord.xy));
	vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0)) - vec3(0.5, 0.5, 0.5);
	outColor.rgb += (vDither.rgb / 255.0);
#endif

//	outColor.a = materialProperties.a < 1.0 ? smoothstep(0.4, 0.0, globalShadowMult): 1.0;
	outColor.a = materialProperties.a < 1.0 ? 0.5 * ( 1.0 - globalShadowMult ) : 1.0;

}
