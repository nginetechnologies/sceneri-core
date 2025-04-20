#version 450

#extension GL_OES_standard_derivatives : enable
#extension GL_ARB_separate_shader_objects : enable

#if TEXTURE_COORDINATES
layout(location = 0) in vec2 fragTexCoord;
#endif

layout(location = 0) out vec4 outColor;

#define MAX_POINTS 4

const uint baseSetIndex = HAS_PUSH_CONSTANTS == 1 ? 0 : 1;
#if HAS_PUSH_CONSTANTS
layout(push_constant) uniform PushConstants
#else
layout (binding = 0) buffer readonly PushConstants
#endif
{
    layout(offset = 32) vec4 color;
    vec2 points[MAX_POINTS];
    uint pointCount;
    float aspectRatio;
    float thickness;
} pushConstants;

#define LINES 300
#define STEP 1.0/float(LINES)

float line(vec2 a, vec2 b, vec2 uv) {
    vec2 v  = b-a;
    vec2 p0 = uv-a;
    float k = min(length(p0)/length(v),1.0);
    vec2 width = fwidth(uv) * pushConstants.thickness;
    float thickness = max(width.x, width.y) * 0.5;
    return smoothstep(thickness, 0.0, length(p0 - k * v));
}

vec2 splineInterpolation(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) 
{
    float alpha = 1.0;
    float tension = 0.0;
    
    float t01 = pow(distance(p0, p1), alpha);
    float t12 = pow(distance(p1, p2), alpha);
    float t23 = pow(distance(p2, p3), alpha);

    vec2 m1 = (1.0f - tension) *
        (p2 - p1 + t12 * ((p1 - p0) / t01 - (p2 - p0) / (t01 + t12)));
    vec2 m2 = (1.0f - tension) *
        (p2 - p1 + t12 * ((p3 - p2) / t23 - (p3 - p1) / (t12 + t23)));
    
    vec2 a = 2.0f * (p1 - p2) + m1 + m2;
    vec2 b = -3.0f * (p1 - p2) - m1 - m1 - m2;
    vec2 c = m1;
    vec2 d = p1;

    return a * t * t * t + b * t * t + c * t + d;
}

float spline(vec2 p0, vec2 p1, vec2 p2, vec2 p3, vec2 uv) 
{
    float curve = 0.0;
    vec2 a = p1;

    for (int i = 1; i <= LINES; i++) 
    {
        vec2 b = splineInterpolation(p0, p1, p2, p3, STEP*float(i));
        curve = mix(curve,1.0, line(a, b, uv));
        a = b;
    }
    
    return curve;
}

void drawSpline(inout vec4 color, vec4 drawColor, vec2 p[MAX_POINTS], uint count, vec2 uv, vec2 aspectRatio) 
{
    for (uint i = 0; i < count - 3; i++) 
    {
        color = mix(color, drawColor, spline(p[i] * aspectRatio, p[i+1] * aspectRatio, p[i+2] * aspectRatio, p[i+3] * aspectRatio, uv));
    }
}

void main()
{
    const vec4 lineColor = pushConstants.color;

    const vec2 aspectRatio = vec2(pushConstants.aspectRatio, 1.0);
    const vec2 adjustedUV = fragTexCoord * aspectRatio;

    vec4 color = vec4(lineColor.rgb, 0.0);
    drawSpline(color, lineColor, pushConstants.points, pushConstants.pointCount, adjustedUV, aspectRatio);

	outColor = color;
}
