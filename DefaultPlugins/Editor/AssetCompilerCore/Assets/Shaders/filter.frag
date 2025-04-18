#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UX3D_MATH_PI 3.1415926535897932384626433832795
#define UX3D_MATH_INV_PI (1.0 / UX3D_MATH_PI)

// enum
const uint cLambertian = 0;
const uint cGGX = 1;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
  float roughness;
  uint sampleCount;
  uint currentMipLevel;
  uint width;
  uint height;
  float lodBias;
  uint distribution; // enum
} pFilterParameters;

layout(set = baseSetIndex, binding = 0) uniform textureCube cubemapTexture;
layout(set = baseSetIndex, binding = 1) uniform sampler cubemapSampler;

layout (location = 0) in vec2 inUV;

// output cubemap faces
layout(location = 0) out vec4 outFace0;
layout(location = 1) out vec4 outFace1;
layout(location = 2) out vec4 outFace2;
layout(location = 3) out vec4 outFace3;
layout(location = 4) out vec4 outFace4;
layout(location = 5) out vec4 outFace5;

void writeFace(uint face, vec3 colorIn)
{
	vec4 color = vec4(colorIn.rgb, 1.0f);

	if(face == 0)
		outFace0 = color;
	else if(face == 1)
		outFace1 = color;
	else if(face == 2)
		outFace2 = color;
	else if(face == 3)
		outFace3 = color;
	else if(face == 4)
		outFace4 = color;
	else //if(face == 5)
		outFace5 = color;
}

vec3 uvToXYZ(uint face, vec2 uv)
{
    if(face == 0)
        return vec3(     1.f,   uv.y,    -uv.x);

    else if(face == 1)
        return vec3(    -1.f,   uv.y,     uv.x);

    else if(face == 2)
        return vec3(   +uv.x,   -1.f,    +uv.y);

    else if(face == 3)
        return vec3(   +uv.x,    1.f,    -uv.y);

    else if(face == 4)
        return vec3(   +uv.x,   uv.y,      1.f);

    else {//if(face == 5)
        return vec3(    -uv.x,  +uv.y,     -1.f);}
}

vec2 dirToUV(vec3 dir)
{
    return vec2(
            0.5f + 0.5f * atan(dir.z, dir.x) / UX3D_MATH_PI,
            1.f - acos(dir.y) / UX3D_MATH_PI);
}

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

// NDF
MicrofacetDistributionSample Lambertian(vec2 xi, float roughness)
{
    MicrofacetDistributionSample lambertian;

    // Cosine weighted hemisphere sampling
    // http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations.html#Cosine-WeightedHemisphereSampling
    lambertian.cosTheta = sqrt(1.0 - xi.y);
    lambertian.sinTheta = sqrt(xi.y); // equivalent to `sqrt(1.0 - cosTheta*cosTheta)`;
    lambertian.phi = 2.0 * UX3D_MATH_PI * xi.x;

    lambertian.pdf = lambertian.cosTheta / UX3D_MATH_PI; // evaluation for solid angle, therefore drop the sinTheta

    return lambertian;
}


// getImportanceSample returns an importance sample direction with pdf in the .w component
vec4 getImportanceSample(uint sampleIndex, vec3 N, float roughness)
{
    // generate a quasi monte carlo point in the unit square [0.1)^2
    vec2 xi = hammersley2d(sampleIndex, pFilterParameters.sampleCount);

    MicrofacetDistributionSample importanceSample;

    // generate the points on the hemisphere with a fitting mapping for
    // the distribution (e.g. lambertian uses a cosine importance)
    if(pFilterParameters.distribution == cLambertian)
    {
        importanceSample = Lambertian(xi, roughness);
    }
    else if(pFilterParameters.distribution == cGGX)
    {
        // Trowbridge-Reitz / GGX microfacet model (Walter et al)
        // https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
        importanceSample = GGX(xi, roughness);
    }

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

// Mipmap Filtered Samples (GPU Gems 3, 20.4)
// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
// https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
float computeLod(float pdf)
{
    // // Solid angle of current sample -- bigger for less likely samples
    // float omegaS = 1.0 / (float(FilterParameters.sampleCoun) * pdf);
    // // Solid angle of texel
    // // note: the factor of 4.0 * UX3D_MATH_PI
    // float omegaP = 4.0 * UX3D_MATH_PI / (6.0 * float(pFilterParameters.width) * float(pFilterParameters.width));
    // // Mip level is determined by the ratio of our sample's solid angle to a texel's solid angle
    // // note that 0.5 * log2 is equivalent to log4
    // float lod = 0.5 * log2(omegaS / omegaP);

    // babylon introduces a factor of K (=4) to the solid angle ratio
    // this helps to avoid undersampling the environment map
    // this does not appear in the original formulation by Jaroslav Krivanek and Mark Colbert
    // log4(4) == 1
    // lod += 1.0;

    // We achieved good results by using the original formulation from Krivanek & Colbert adapted to cubemaps

    // https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
    float lod = 0.5 * log2( 6.0 * float(pFilterParameters.width) * float(pFilterParameters.height) / (float(pFilterParameters.sampleCount) * pdf));


    return lod;
}

vec3 filterColor(vec3 N)
{
    vec3 color = vec3(0.f);
    float weight = 0.0f;

    const uint sampleCount = pFilterParameters.sampleCount;
    for(uint i = 0; i < sampleCount; ++i)
    {
        vec4 importanceSample = getImportanceSample(i, N, pFilterParameters.roughness);

        vec3 H = vec3(importanceSample.xyz);
        float pdf = importanceSample.w;

        // mipmap filtered samples (GPU Gems 3, 20.4)
        float lod = computeLod(pdf);

        // apply the bias to the lod
        lod += pFilterParameters.lodBias;

        if(pFilterParameters.distribution == cLambertian)
        {
            // sample lambertian at a lower resolution to avoid fireflies
            vec3 lambertian = textureLod(samplerCube(cubemapTexture, cubemapSampler), H, lod).rgb;

            //// the below operations cancel each other out
            // lambertian *= NdotH; // lamberts law
            // lambertian /= pdf; // invert bias from importance sampling
            // lambertian /= UX3D_MATH_PI; // convert irradiance to radiance https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/

            color += lambertian;
        }
        else if(pFilterParameters.distribution == cGGX)
        {
            // Note: reflect takes incident vector.
            vec3 V = N;
            vec3 L = normalize(reflect(-V, H));
            float NdotL = dot(N, L);

            if (NdotL > 0.0)
            {
                if(pFilterParameters.roughness == 0.0)
                {
                    // without this the roughness=0 lod is too high
                    lod = pFilterParameters.lodBias;
                }
                vec3 sampleColor = textureLod(samplerCube(cubemapTexture, cubemapSampler), L, lod).rgb;
                color += sampleColor * NdotL;
                weight += NdotL;
            }
        }
    }

    if(weight != 0.0f)
    {
        color /= weight;
    }
    else
    {
        color /= float(pFilterParameters.sampleCount);
    }

    return color.rgb ;
}

// entry point
void main()
{
    const uint currentMipLevel = pFilterParameters.currentMipLevel;
	vec2 newUV = inUV * float(1 << currentMipLevel);

	newUV = newUV*2.0-1.0;

	for(uint face = 0; face < 6; ++face)
	{
		vec3 scan = uvToXYZ(face, newUV);

		vec3 direction = normalize(scan);
		direction.y = -direction.y;

		writeFace(face, filterColor(direction));
	}
}
