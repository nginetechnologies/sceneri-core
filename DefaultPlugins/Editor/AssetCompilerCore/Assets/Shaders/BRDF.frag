#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UX3D_MATH_PI 3.1415926535897932384626433832795
#define UX3D_MATH_INV_PI (1.0 / UX3D_MATH_PI)

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
  uint sampleCount;
} pFilterParameters;

layout (location = 0) in vec2 inUV;

// output cubemap faces
layout(location = 0) out vec2 outLUT;

float saturate(float v)
{
    return clamp(v, 0.0f, 1.0f);
}

// Hammersley Points on the Hemisphere
// CC BY 3.0 (Holger Dammertz)
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// with adapted interface
float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// hammersley2d describes a sequence of points in the 2d unit square [0,1)^2
// that can be used for quasi Monte Carlo integration
vec2 hammersley2d(uint i, uint N) {
    return vec2(float(i)/float(N), radicalInverse_VdC(i));
}

// Hemisphere Sample

// TBN generates a tangent bitangent normal coordinate frame from the normal
// (the normal must be normalized)
mat3 generateTBN(vec3 normal)
{
    vec3 bitangent = vec3(0.0, 1.0, 0.0);

    float NdotUp = dot(normal, vec3(0.0, 1.0, 0.0));
    float epsilon = 0.00001;
    if (1.0 - abs(NdotUp) <= epsilon)
    {
        // Sampling +Y or -Y, so we need a more robust bitangent.
        if (NdotUp > 0.0)
        {
            bitangent = vec3(0.0, 0.0, 1.0);
        }
        else
        {
            bitangent = vec3(0.0, 0.0, -1.0);
        }
    }

    vec3 tangent = normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);

    return mat3(tangent, bitangent, normal);
}

struct MicrofacetDistributionSample
{
    float pdf;
    float cosTheta;
    float sinTheta;
    float phi;
};

float D_GGX(float NdotH, float roughness) {
    float a = NdotH * roughness;
    float k = roughness / (1.0 - NdotH * NdotH + a * a);
    return k * k * (1.0 / UX3D_MATH_PI);
}

// GGX microfacet distribution
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
// This implementation is based on https://bruop.github.io/ibl/,
//  https://www.tobias-franke.eu/log/2014/03/30/notes_on_importance_sampling.html
// and https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
MicrofacetDistributionSample GGX(vec2 xi, float roughness)
{
    MicrofacetDistributionSample ggx;

    // evaluate sampling equations
    float alpha = roughness * roughness;
    ggx.cosTheta = saturate(sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y)));
    ggx.sinTheta = sqrt(1.0 - ggx.cosTheta * ggx.cosTheta);
    ggx.phi = 2.0 * UX3D_MATH_PI * xi.x;

    // evaluate GGX pdf (for half vector)
    ggx.pdf = D_GGX(ggx.cosTheta, alpha);

    // Apply the Jacobian to obtain a pdf that is parameterized by l
    // see https://bruop.github.io/ibl/
    // Typically you'd have the following:
    // float pdf = D_GGX(NoH, roughness) * NoH / (4.0 * VoH);
    // but since V = N => VoH == NoH
    ggx.pdf /= 4.0;

    return ggx;
}

// getImportanceSample returns an importance sample direction with pdf in the .w component
vec4 getImportanceSample(uint sampleIndex, vec3 N, float roughness)
{
    // generate a quasi monte carlo point in the unit square [0.1)^2
    vec2 xi = hammersley2d(sampleIndex, pFilterParameters.sampleCount);

    MicrofacetDistributionSample importanceSample;

    // generate the points on the hemisphere with a fitting mapping for
    // the distribution (e.g. lambertian uses a cosine importance)
   // Trowbridge-Reitz / GGX microfacet model (Walter et al)
	// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
	importanceSample = GGX(xi, roughness);

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    vec3 localSpaceDirection = normalize(vec3(
        importanceSample.sinTheta * cos(importanceSample.phi),
        importanceSample.sinTheta * sin(importanceSample.phi),
        importanceSample.cosTheta
    ));
    mat3 TBN = generateTBN(N);
    vec3 direction = TBN * localSpaceDirection;

    return vec4(direction, importanceSample.pdf);
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
    float a2 = pow(roughness, 4.0);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

// Compute LUT for GGX distribution.
// See https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec2 LUT(float NdotV, float roughness)
{
    // Compute spherical view vector: (sin(phi), 0, cos(phi))
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    // The macro surface normal just points up.
    vec3 N = vec3(0.0, 0.0, 1.0);

    // To make the LUT independant from the material's F0, which is part of the Fresnel term
    // when substituted by Schlick's approximation, we factor it out of the integral,
    // yielding to the form: F0 * I1 + I2
    // I1 and I2 are slighlty different in the Fresnel term, but both only depend on
    // NoL and roughness, so they are both numerically integrated and written into two channels.
    float A = 0.0;
    float B = 0.0;

    const uint sampleCount = pFilterParameters.sampleCount;
    for(uint i = 0; i < sampleCount; ++i)
    {
        // Importance sampling, depending on the distribution.
        vec4 importanceSample = getImportanceSample(i, N, roughness);
        vec3 H = importanceSample.xyz;
        // float pdf = importanceSample.w;
        vec3 L = normalize(reflect(-V, H));

        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        if (NdotL > 0.0)
        {
			// LUT for GGX distribution.

			// Taken from: https://bruop.github.io/ibl
			// Shadertoy: https://www.shadertoy.com/view/3lXXDB
			// Terms besides V are from the GGX PDF we're dividing by.
			float V_pdf = V_SmithGGXCorrelated(NdotV, NdotL, roughness) * VdotH * NdotL / NdotH;
			float Fc = pow(1.0 - VdotH, 5.0);
			A += (1.0 - Fc) * V_pdf;
			B += Fc * V_pdf;
        }
    }

    // The PDF is simply pdf(v, h) -> NDF * <nh>.
    // To parametrize the PDF over l, use the Jacobian transform, yielding to: pdf(v, l) -> NDF * <nh> / 4<vh>
    // Since the BRDF divide through the PDF to be normalized, the 4 can be pulled out of the integral.
	return vec2(4.0 * A, 4.0 * B) / float(sampleCount);
}

// entry point
void main()
{
	// Write LUT:
	// x-coordinate: NdotV
	// y-coordinate: roughness
	outLUT = LUT(inUV.x, 1.0 - inUV.y);
}
