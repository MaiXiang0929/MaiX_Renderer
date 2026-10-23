#version 400 core

layout(location = 0) in vec3 pos;
layout(location = 2) in vec2 texCoord;
out vec2 fragTexCoord;
uniform mat4 lightMvp;

void main()
{
    fragTexCoord = texCoord;
    gl_Position = lightMvp * vec4(pos, 1.0);
}
