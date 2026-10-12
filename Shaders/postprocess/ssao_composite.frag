#version 330 core
in vec2 fragTexCoord;
layout(location = 0) out vec4 color;
uniform sampler2D sceneTexture;
uniform sampler2D aoTexture;
uniform float intensity;
void main()
{
    vec4 scene = texture(sceneTexture, fragTexCoord);
    float ao = clamp(texture(aoTexture, fragTexCoord).r, 0.0, 1.0);
    float intensityScale = clamp(intensity, 0.0, 3.0);
    float occlusion = clamp((1.0 - ao) * intensityScale, 0.0, 1.0);
    color = vec4(scene.rgb * (1.0 - occlusion), scene.a);
}
