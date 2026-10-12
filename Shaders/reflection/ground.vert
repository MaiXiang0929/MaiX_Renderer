// 反射地面顶点着色器
// 将世界空间位置传入片段着色器，用于投影到反射纹理和 cubemap 采样

#version 330 core

layout (location = 0) in vec3 pos;

uniform mat4 mvp;
uniform mat4 model;

out vec3 worldPos;

void main()
{
    vec4 wp = model * vec4(pos, 1.0);
    worldPos = wp.xyz / wp.w;
    gl_Position = mvp * vec4(pos, 1.0);
}
