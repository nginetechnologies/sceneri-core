#version 450

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
	vec2(1.0, -1.0),
    vec2(1.0, 1.0),
	vec2(-1.0, 1.0)
);

void main()
{
	vec2 position = positions[gl_VertexIndex];

	gl_Position = vec4(position, 0.0, 1.0);
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
}
