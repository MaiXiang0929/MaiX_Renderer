#version 400 core

layout(vertices = 3) out;

in VS_OUT
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
} tcIn[];

out TC_OUT
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
} tcOut[];

uniform float tessellationLevel;

void main()
{
    tcOut[gl_InvocationID].position = tcIn[gl_InvocationID].position;
    tcOut[gl_InvocationID].normal = tcIn[gl_InvocationID].normal;
    tcOut[gl_InvocationID].uv = tcIn[gl_InvocationID].uv;
    tcOut[gl_InvocationID].tangent = tcIn[gl_InvocationID].tangent;

    if (gl_InvocationID == 0) {
        float level = clamp(tessellationLevel, 1.0, 64.0);
        gl_TessLevelInner[0] = level;
        gl_TessLevelOuter[0] = level;
        gl_TessLevelOuter[1] = level;
        gl_TessLevelOuter[2] = level;
    }
}
