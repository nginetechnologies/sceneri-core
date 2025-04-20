#version 450

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec2 outProjectionPos;

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0),
	vec2(1.0, 1.0),
    vec2(1.0, -1.0),
	vec2(-1.0, -1.0)
);

vec2 textureCoordinates[6] = vec2[](
	vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
	vec2(1.0, 1.0),
    vec2(1.0, 0.0),
	vec2(0.0, 0.0)
);

void main()
{
	vec2 position = positions[gl_VertexIndex];

	gl_Position = vec4(position, 0.0, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif

	outTexCoord = textureCoordinates[gl_VertexIndex];
}
