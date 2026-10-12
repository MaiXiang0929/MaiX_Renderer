#version 330 core

in vec2 fragTexCoord;
layout(location = 0) out vec4 color;

uniform sampler2D sceneTexture;
uniform float threshold;

void main()
{
    vec3 sceneColor = max(texture(sceneTexture, fragTexCoord).rgb, vec3(0.0));
    float brightness = max(max(sceneColor.r, sceneColor.g), sceneColor.b);
    float contribution = max(brightness - threshold, 0.0) /
        max(brightness, 0.0001);
    color = vec4(sceneColor * contribution, 1.0);
}
