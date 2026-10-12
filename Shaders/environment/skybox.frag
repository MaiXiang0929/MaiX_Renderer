// Skybox 片段着色器
// 使用插值的立方体顶点位置作为方向向量采样 cubemap

#version 330 core

in vec3 texCoords;

uniform samplerCube skybox;

layout(location = 0) out vec4 color;

void main()
{
    color = texture(skybox, texCoords);
}
