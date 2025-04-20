#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 1) in vec3 inNormal;
layout(location = 4) in vec4 inPreviousPosition;
layout(location = 5) in vec4 inCurrentPosition;

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;

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

layout(std140, set = baseSetIndex + 0, binding = 0) uniform readonly ViewInfo
{
	PerViewInfo current;
	PerViewInfo previous;
} viewInfo;

vec2 CalcVelocity(vec4 newPos, vec4 oldPos)
{
	newPos.xy -= (viewInfo.current.jitterOffset.xy * newPos.w);
	oldPos.xy -= (viewInfo.previous.jitterOffset.xy * oldPos.w);
	
	newPos.xy /= newPos.w;
	oldPos.xy /= oldPos.w;

	newPos.xy = (vec2(1.0) + newPos.xy) * vec2(0.5);
	oldPos.xy = (vec2(1.0) + oldPos.xy) * vec2(0.5);

	vec2 delta = newPos.xy - oldPos.xy;

	return delta.xy;
}

#if OUTPUT_DEPTH
//layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;
layout(location = 2) out vec4 outNormal;
//layout(location = 3) out vec4 outMaterialProperties;
layout(location = 4) out vec2 taaVelocity;
#else
layout(location = 1) out vec4 outNormal;
layout(location = 3) out vec2 taaVelocity;
#endif


void main()
{
#if OUTPUT_DEPTH
	outDepth = gl_FragCoord.z;
#endif
	outNormal.xyz = normalize(inNormal)*0.5+0.5;

	taaVelocity = CalcVelocity(inCurrentPosition, inPreviousPosition);
}
