// 顶点着色器，用于渲染始终面向相机的图标（billboard）

#version 400 core
layout(location = 0) in vec2 offset;
layout(location = 1) in vec2 texCoord;

uniform mat4 viewProjection;
uniform vec3 worldPosition;
uniform float viewportWidth;
uniform float viewportHeight;
uniform float iconSizePixels;

out vec2 TexCoord;

void main()
{
    vec4 centerClip = viewProjection * vec4(worldPosition, 1.0);
    vec2 viewport = max(vec2(viewportWidth, viewportHeight), vec2(1.0));
    vec2 clipOffset = offset * iconSizePixels * 2.0 / viewport * centerClip.w;
    gl_Position = centerClip + vec4(clipOffset, 0.0, 0.0);
    TexCoord = texCoord;
}
