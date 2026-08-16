#version 330 core
in vec2 fragTexCoord;
layout(location = 0) out vec4 color;
uniform sampler2D sceneTexture;
uniform sampler2D aoTexture;
uniform float intensity;
void main()
{
    vec4 scene = texture(sceneTexture, fragTexCoord);
    float ao = texture(aoTexture, fragTexCoord).r;
    color = vec4(scene.rgb * mix(1.0, ao, clamp(intensity, 0.0, 3.0)), scene.a);
}
