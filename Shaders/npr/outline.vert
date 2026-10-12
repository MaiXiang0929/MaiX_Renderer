#version 400 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;

uniform mat4 projection;
uniform mat4 modelView;
uniform mat3 normalMatrix;
uniform float outlineThickness;

void main()
{
    vec4 viewPosition = modelView * vec4(pos, 1.0);
    vec3 viewNormal = normalize(normalMatrix * normal);
    viewPosition.xyz += viewNormal * outlineThickness;
    gl_Position = projection * viewPosition;
}
