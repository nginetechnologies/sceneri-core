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
	layout(offset = 32) vec4 backgroundColor;
    vec4 gridColor;
    vec2 offset;
    float aspectRatio;
} pushConstants;

#if SAMPLE_TEXTURE
layout(set = baseSetIndex, binding = 0) uniform texture2D sampledTexture;
layout(set = baseSetIndex, binding = 1) uniform sampler textureSampler;
#endif

void main()
{
    vec4 color = pushConstants.backgroundColor;

    if(color.a == 0)
    {
        discard;
    }

    const vec2 aspectRatio = vec2(pushConstants.aspectRatio, 1.0);
    vec2 uv = fragTexCoord * aspectRatio; 
    uv += pushConstants.offset;

    // INITIAL DOTS
    float divisions = 30.;

    // Create grid
    vec2 dst = uv * divisions;

    // Divide into squares
    dst = mod(dst, vec2(1.0) * aspectRatio);
    
    // Create circles
    float c;
    c = distance(dst, vec2(.5) * aspectRatio);

    const float size = 0.01f;
    const float hardness = 0.05f;
    c = smoothstep(size, size + hardness, c);

    vec3 f = mix(pushConstants.backgroundColor.rgb, pushConstants.gridColor.rgb, 1. - c);
    color = vec4(f, 1.0f);

    outColor = color;
}
