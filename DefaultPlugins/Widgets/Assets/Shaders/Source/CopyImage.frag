#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform texture2D copiedTexture;
layout(binding = 1) uniform sampler textureSampler;

void main()
{
	vec4 color = texture(sampler2D(copiedTexture, textureSampler), fragTexCoord);
	if(color.a == 0.0)
	{
		discard;
	}

    outColor.rgb = color.rgb;
	outColor.a = 1.0 - color.a;
}
