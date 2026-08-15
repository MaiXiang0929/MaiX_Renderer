#version 400 core

layout(location = 0) out vec4 color;

uniform vec3 lineColor;

void main()
{
    const float alpha = 0.65;
    color = vec4(lineColor * alpha, alpha);
}
