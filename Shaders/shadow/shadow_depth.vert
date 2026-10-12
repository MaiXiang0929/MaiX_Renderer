#version 400 core

layout(location = 0) in vec3 pos;
uniform mat4 lightMvp;

void main()
{
    gl_Position = lightMvp * vec4(pos, 1.0);
}
