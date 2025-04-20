#version 450

struct PerViewInfo
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 viewProjectionMatrixCylinderBillboard;
	mat4 viewProjectionMatrixSphericalBillboard;
	mat4 invertedViewProjectionMatrix;
	mat4 invertedViewMatrix;
	mat4 invertedProjectionMatrix;

	vec4 viewLocationAndTime;
	mat3 viewRotation;
	mat3 invertedViewRotation;
	uvec4 renderAndOutputResolution;
	vec4 jitterOffset;
};

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
layout(std140, set = baseSetIndex, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

layout(location = 0) out vec3 outViewDirection;

void main()
{
	vec2 uv = vec2(-1.0) + ((ivec2(gl_VertexIndex, gl_VertexIndex) & ivec2(1, 2)) << ivec2(2, 1));
	const vec4 position = vec4(uv, 0.0, 1.0);

	mat4 inverseProjection = viewInfo.current.invertedProjectionMatrix;
    mat3 inverseModelview = transpose(mat3(viewInfo.current.viewMatrix));
    vec3 unprojected = (inverseProjection * position).xyz;
    outViewDirection = inverseModelview * unprojected;

	// TODO: Move this to shader compiler
	mat3 rot = mat3(vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, 1, 0));
	outViewDirection = rot * outViewDirection;

	gl_Position = position;
#if RENDERER_WEBGPU
    gl_Position.y = -gl_Position.y;
#endif
}
