#version 400 core

layout(location = 0) out vec4 color;

uniform vec3 outlineColor;

void main()
{
    vec3 linearColor = pow(max(outlineColor, vec3(0.0)), vec3(2.2));
    color = vec4(linearColor, 1.0);
}
