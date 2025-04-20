#version 450

#extension GL_OES_standard_derivatives : enable
#extension GL_ARB_separate_shader_objects : enable

#if TEXTURE_COORDINATES
layout(location = 0) in vec2 fragTexCoord;
#endif

layout(location = 0) out vec4 outColor;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 32) uint color;
#if LINEAR_GRADIENT
	uint gradientColor[3];
#elif CONIC_GRADIENT
	uint gradientColor[8];
#endif

#if ROUNDED_CORNERS
	float radius;
    float aspectRatio;
#elif INDIVIDUAL_ROUNDED_CORNERS
    vec4 radius;
    float aspectRatio;
#endif
#if INNER_BORDER
	vec2 innerSize;
#endif

#if LINEAR_GRADIENT
    vec2 point[3];
#elif CONIC_GRADIENT
    float point[9];
#endif
} pushConstants;

#if SAMPLE_TEXTURE
layout(set = baseSetIndex, binding = 0) uniform texture2D sampledTexture;
layout(set = baseSetIndex, binding = 1) uniform sampler textureSampler;
#endif

#if CIRCLE
float aastep(float threshold, float value) {
  #ifdef GL_OES_standard_derivatives
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678118654757;
    return smoothstep(threshold-afwidth, threshold+afwidth, value);
  #else
    return step(threshold, value);
  #endif
}

float circle(vec2 st)
{
    return aastep(0.5, length(st - vec2(0.5)));
}
#elif ROUNDED_CORNERS
// from http://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) 
{
    vec2 q = abs(CenterPosition) - Size + Radius;
    return length(max(q, 0.0)) - Radius;
}
#elif INDIVIDUAL_ROUNDED_CORNERS
// with four individual corner sizes
float sdRoundBox(vec2 CenterPosition, vec2 Size, vec4 r)
{
    r.xy = (CenterPosition.x>0.0)?r.xy : r.zw;
    r.x  = (CenterPosition.y>0.0)?r.x  : r.y;

    vec2 q = abs(CenterPosition) - Size + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}
#else
float boxSDF(vec2 p, in vec2 b)
{
   p = abs(p) - b;
   return length(max(p, 0.0)) + min(max(p.x, p.y), 0.0);
}
#endif

#if INNER_BORDER
float intersect(float shape1, float shape2){
    return max(shape1, shape2);
}

float subtract(float base, float subtraction){
    return intersect(base, -subtraction);
}
#endif

#if LINEAR_GRADIENT || CONIC_GRADIENT

#define SRGB_TO_LINEAR(c) pow((c), vec3(2.2))
#define LINEAR_TO_SRGB(c) pow((c), vec3(1.0 / 2.2))

// Gradient noise from Jorge Jimenez's presentation:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float gradientNoise(in vec2 uv)
{
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(uv, magic.xy)));
}

vec4 gradient(vec2 uv, vec2 a, vec2 b, vec4 color0, vec4 color1)
{
    const vec2 ba = b - a;
    float t = dot(uv - a, ba) / dot(ba, ba);
    t = smoothstep(0.0, 1.0, clamp(t, 0.0, 1.0));
    return mix(color0, color1, t);
}

vec2 rotate(vec2 st, float angle) {
    vec2 sinCos = vec2(sin(angle), cos(angle));
    return vec2(
        st.x * sinCos.y - st.y * sinCos.x,
        st.x * sinCos.x + st.y * sinCos.y
    );
}

#endif

vec4 unpackColor(uint packedColor)
{
    const uvec4 shifts = uvec4(24, 16, 8, 0);
    uvec4 packedColors = (uvec4(packedColor) >> shifts) & uvec4(0xFF);
    return vec4(packedColors) / 255.0;
}

void main()
{
	vec4 color = unpackColor(pushConstants.color);

#if SAMPLE_TEXTURE
	color *= texture(sampler2D(sampledTexture, textureSampler), fragTexCoord).rgba;
#endif

#if CIRCLE
	color.a *= 1.0 - circle(fragTexCoord);
#elif ROUNDED_CORNERS
    const vec2 aspectRatio = vec2(pushConstants.aspectRatio, 1.0);
    const vec2 adjustedUV = fragTexCoord * aspectRatio;
    const vec2 boxOffset = vec2(0.5) * aspectRatio;

	const float outerBox = roundedBoxSDF(adjustedUV - boxOffset, boxOffset, pushConstants.radius);
	const float edgeSoftness = 0.0006f;

#if INNER_BORDER
	const float innerBox = roundedBoxSDF(adjustedUV - boxOffset, pushConstants.innerSize * aspectRatio, pushConstants.radius);
	const float distance = subtract(outerBox, innerBox);
#else
	const float distance = outerBox;
#endif

	const float alpha = 1.f - smoothstep(0.f, edgeSoftness * 2.f, distance);
	color.a *= alpha;

#elif INDIVIDUAL_ROUNDED_CORNERS
    const vec2 aspectRatio = vec2(pushConstants.aspectRatio, 1.0);
    const vec2 adjustedUV = fragTexCoord * aspectRatio;
    const vec2 boxOffset = vec2(0.5) * aspectRatio;

    const float outerBox = sdRoundBox(adjustedUV - boxOffset, boxOffset, pushConstants.radius);
	const float edgeSoftness = 0.0006f;

#if INNER_BORDER
	const float innerBox = sdRoundBox(adjustedUV - boxOffset, pushConstants.innerSize * aspectRatio, pushConstants.radius);
	const float distance = subtract(outerBox, innerBox);
#else
	const float distance = outerBox;
#endif

	const float alpha = 1.f - smoothstep(0.f, edgeSoftness * 2.f, distance);
	color.a *= alpha;

#elif INNER_BORDER 
	const float edgeSoftness = 0.0006f;
	const float outerBox = boxSDF(fragTexCoord - vec2(0.5), vec2(0.5));
	const float innerBox = boxSDF(fragTexCoord - vec2(0.5), pushConstants.innerSize);
	const float distance = subtract(outerBox, innerBox);
	const float alpha = 1.f - smoothstep(0.f, edgeSoftness * 2.f, distance);
	color.a *= alpha;
#endif

	if(color.a == 0)
	{
		discard;
	}

#if LINEAR_GRADIENT
 	// Move uv space so it goes from bottom to top like in css
    vec2 uv = fragTexCoord;
    uv.y = abs(1.0f - uv.y);

    const vec4 sourceColor0 = unpackColor(pushConstants.gradientColor[0]);
    const vec4 sourceColor1 = unpackColor(pushConstants.gradientColor[1]);
    const vec4 sourceColor2 = unpackColor(pushConstants.gradientColor[2]);

    const vec4 color0 = vec4(SRGB_TO_LINEAR(sourceColor0.xyz), sourceColor0.a);
    const vec4 color1 = vec4(SRGB_TO_LINEAR(sourceColor1.xyz), sourceColor1.a);
    const vec4 color2 = vec4(SRGB_TO_LINEAR(sourceColor2.xyz), sourceColor2.a);
    vec4 gradientColor = gradient(uv, pushConstants.point[0], pushConstants.point[1], color0, color1);
    if(color2.a > 0)
        gradientColor = gradient(uv, pushConstants.point[1], pushConstants.point[2], gradientColor, color2);

    gradientColor.rgb = LINEAR_TO_SRGB(gradientColor.rgb);
    // Add gradient noise to reduce banding.
    gradientColor += (1.0/255.0) * gradientNoise(uv) - (0.5/255.0);

	color.rgb = gradientColor.rgb;
	color.a *= gradientColor.a;
#elif CONIC_GRADIENT
 	// Move uv space so it goes from bottom to top like in css
    vec2 uv = fragTexCoord; 
    uv.y = abs(1.0f - uv.y);

    vec2 gradientOrigin = vec2(0.5);
    uv = (uv - gradientOrigin);

    const float pi = 3.14159265359;
    uv = rotate(uv, (pi/2.0) + pushConstants.point[0]);

    float angleRatio = ((-atan(uv.y, uv.x)) + pi) / 2.0 / pi;
    for (uint i = 0; i < 7; ++i)
    {
        float start = pushConstants.point[i + 1];
        float end = pushConstants.point[i + 2];
        if (angleRatio >= start && angleRatio < end)
        {
            float t = (angleRatio - start) / (end - start);

            const vec4 sourceColor = unpackColor(pushConstants.gradientColor[i]);
            const vec4 nextSourceColor = unpackColor(pushConstants.gradientColor[i + 1]);
            color = mix(sourceColor, nextSourceColor, t);
            break;
        }
    }

#endif

	outColor = color;
}
