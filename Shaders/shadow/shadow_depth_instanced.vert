#version 400 core

layout(location = 0) in vec3 pos;
layout(location = 2) in vec2 texCoord;
out vec2 fragTexCoord;
out vec3 shadowWorldPosition;
uniform mat4 inverseLightVP;

layout(std140) uniform InstanceTransforms
{
    mat4 instanceLightMvp[256];
};

void main()
{
    fragTexCoord = texCoord;
    gl_Position = instanceLightMvp[gl_InstanceID] * vec4(pos, 1.0);
    vec4 world = inverseLightVP * gl_Position;
    shadowWorldPosition = world.xyz / world.w;
}
