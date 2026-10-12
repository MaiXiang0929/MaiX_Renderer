// 片段着色器，用于渲染一个圆形的黄色 billboard

#version 400 core
in vec2 TexCoord;
layout(location = 0) out vec4 color;

uniform vec3 iconColor;

void main()
{
    float dist = distance(TexCoord, vec2(0.5, 0.5));
    if (dist > 0.5) discard; // 裁剪成一个圆
    
    float edge = 1.0 - smoothstep(0.42, 0.5, dist);
    color = vec4(iconColor * edge, edge);
}
