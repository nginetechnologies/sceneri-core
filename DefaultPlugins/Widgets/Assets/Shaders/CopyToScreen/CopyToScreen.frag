#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform texture2D copiedTexture;
layout(binding = 1) uniform sampler textureSampler;

void main()
{
	outColor = texture(sampler2D(copiedTexture, textureSampler), inTexCoord);
}
