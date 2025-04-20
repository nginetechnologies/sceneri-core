#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 32) uint color;
	vec2 uvOrigin;
	vec2 uvScale;
#if LINEAR_GRADIENT
	vec2 uvGradientScale;
	vec2 uvGradientOffset;
	uint gradientColor[3];
    vec2 point[3];
#endif
} pushConstants;

layout(set = baseSetIndex, set = baseSetIndex, binding = 0) uniform texture2D sampledTexture;
layout(set = baseSetIndex, set = baseSetIndex, binding = 1) uniform sampler textureSampler;

#if LINEAR_GRADIENT

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

#endif

vec4 unpackColor(uint packedColor)
{
    const uvec4 shifts = uvec4(24, 16, 8, 0);
    uvec4 packedColors = (uvec4(packedColor) >> shifts) & uvec4(0xFF);
    return vec4(packedColors) / 255.0;
}

void main()
{
	vec2 textureCoordinate = pushConstants.uvOrigin + pushConstants.uvScale * fragTexCoord;
	float distance = texture(sampler2D(sampledTexture, textureSampler), textureCoordinate).r;
	float smoothWidth = fwidth(distance);

	vec4 sourceColor = unpackColor(pushConstants.color);
	float alpha = smoothstep(0.5 - smoothWidth, 0.5 + smoothWidth, distance) * sourceColor.a;
	vec4 color = vec4(sourceColor.rgb, 1.0);
	color.a *= alpha;
	if(color.a == 0.0)
	{
		discard;
	}


#if LINEAR_GRADIENT
 	// Move uv space so it goes from bottom to top like in css
    vec2 uv = fragTexCoord;
    uv.y = abs(1.0f - uv.y);

	uv.x = uv.x * pushConstants.uvGradientScale.x + pushConstants.uvGradientOffset.x;

	vec4 sourceColor0 = unpackColor(pushConstants.gradientColor[0]);
	vec4 sourceColor1 = unpackColor(pushConstants.gradientColor[1]);
	vec4 sourceColor2 = unpackColor(pushConstants.gradientColor[2]);

    const vec4 color0 = vec4(SRGB_TO_LINEAR(sourceColor0.xyz), sourceColor0.a);
    const vec4 color1 = vec4(SRGB_TO_LINEAR(sourceColor1.xyz), sourceColor1.a);
    const vec4 color2 = vec4(SRGB_TO_LINEAR(sourceColor2.xyz), sourceColor2.a);
    vec4 gradientColor = gradient(uv, pushConstants.point[0], pushConstants.point[1], color0, color1);
    if(sourceColor2.a > 0)
        gradientColor = gradient(uv, pushConstants.point[1], pushConstants.point[2], gradientColor, color2);

    gradientColor.rgb = LINEAR_TO_SRGB(gradientColor.rgb);
    // Add gradient noise to reduce banding.
    gradientColor += (1.0/255.0) * gradientNoise(uv) - (0.5/255.0);

	color.rgb = gradientColor.rgb;
	color.a *= gradientColor.a;
#endif

	outColor = vec4(vec3(alpha) * color.rgb, color.a);
}
