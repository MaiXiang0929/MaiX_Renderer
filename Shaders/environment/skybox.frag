// Skybox 片段着色器
// 使用插值的立方体顶点位置作为方向向量采样 cubemap

#version 330 core

in vec3 texCoords;

uniform samplerCube skybox;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 indirectDiffuse;


// Inverse yaw maps a world-space direction into the authored environment.
uniform vec3 environmentTransform; // cos(yaw), sin(yaw), linear intensity
vec3 EnvironmentDirection(vec3 d)
{
    return vec3(environmentTransform.x*d.x - environmentTransform.y*d.z,
        d.y, environmentTransform.y*d.x + environmentTransform.x*d.z);
}

void main()
{
    indirectDiffuse = vec4(0.0);
    color = vec4(min(texture(skybox, EnvironmentDirection(texCoords)).rgb * environmentTransform.z, vec3(65504.0)), 1.0);
}
