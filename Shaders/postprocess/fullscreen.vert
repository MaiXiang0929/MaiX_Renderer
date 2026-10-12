#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 texCoord;

out vec2 fragTexCoord;

void main()
{
    gl_Position = vec4(pos.xy, 0.0, 1.0);
    fragTexCoord = texCoord;
}
