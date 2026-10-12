#version 400 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

out VS_OUT
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
} vsOut;

void main()
{
    vsOut.position = pos;
    vsOut.normal = normal;
    vsOut.uv = texCoord;
    vsOut.tangent = tangent;
    gl_Position = vec4(pos, 1.0);
}
