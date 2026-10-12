#version 400 core

layout(location = 0) in vec3 pos;

layout(std140) uniform InstanceTransforms
{
    mat4 instanceLightMvp[256];
};

void main()
{
    gl_Position = instanceLightMvp[gl_InstanceID] * vec4(pos, 1.0);
}
