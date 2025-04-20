#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform texture2D sampledTexture;
layout(set = 1, binding = 1) uniform sampler textureSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sampler2D(sampledTexture, textureSampler), fragTexCoord);
}
