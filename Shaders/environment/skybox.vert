// Skybox 顶点着色器
// 将立方体的顶点位置作为 cubemap 采样方向传入片段着色器
// 使用 .xyww 技巧使天空盒始终位于远平面

#version 330 core

layout (location = 0) in vec3 pos;

uniform mat4 projection;
uniform mat4 view;

out vec3 texCoords;

void main()
{
    texCoords = pos;

    vec4 clipPos = projection * view * vec4(pos, 1.0);
    // .xyww 技巧：将 z 强制设为 w，透视除法后深度恒为 1.0（远平面）
    gl_Position = clipPos.xyww;
}
