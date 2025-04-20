#version 450

layout(location = 0) out vec2 fragTexCoord;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset =0) vec2 positionRatio;
	layout(offset = 8) vec2 sizeRatio;
	layout(offset = 16) float depth;
} pushConstants;

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
	vec2(1.0, -1.0),
    vec2(1.0, 1.0),
	vec2(-1.0, 1.0)
);

vec2 textureCoordinates[6] = vec2[](
	vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
	vec2(1.0, 0.0),
    vec2(1.0, 1.0),
	vec2(0.0, 1.0)
);

void main()
{
	fragTexCoord = textureCoordinates[gl_VertexIndex];

	vec2 position = pushConstants.sizeRatio * sign(sign(positions[gl_VertexIndex]) + 1.0);
	position += pushConstants.positionRatio * abs(sign(sign(positions[gl_VertexIndex]) - 1.0));
#if RENDERER_WEBGPU
    position.y =-position.y;
#endif

	gl_Position = vec4(position, pushConstants.depth, 1.0);
}
