#version 450

#if TEXTURE_COORDINATES
layout(location = 0) out vec2 fragTexCoord;
#endif

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
	layout(offset = 0) vec2 positionRatio;
	layout(offset = 8) vec2 sizeRatio;
	layout(offset = 16) float angle;
	layout(offset = 20) float depth;
} pushConstants;

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
	vec2(1.0, -1.0),
    vec2(1.0, 1.0),
	vec2(-1.0, 1.0)
);

#if TEXTURE_COORDINATES
vec2 textureCoordinates[6] = vec2[](
	vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
	vec2(1.0, 0.0),
    vec2(1.0, 1.0),
	vec2(0.0, 1.0)
);
#endif

void main()
{
#if TEXTURE_COORDINATES
	vec2 texCoord = textureCoordinates[gl_VertexIndex];
	const float angle = pushConstants.angle;
	const float sinFactor = sin(angle);
	const float cosFactor = cos(angle);

	texCoord -= vec2(0.5);
	texCoord *= mat2(cosFactor, sinFactor, -sinFactor, cosFactor);
	texCoord += vec2(0.5);

	fragTexCoord = texCoord;
#endif

	vec2 position = pushConstants.sizeRatio * sign(sign(positions[gl_VertexIndex]) + 1.0);
	position += pushConstants.positionRatio * abs(sign(sign(positions[gl_VertexIndex]) - 1.0));

#if RENDERER_WEBGPU
	position.y = -position.y;
#endif

	gl_Position = vec4(position, pushConstants.depth, 1.0);
}
