#version 330 core

in vec2 fragTexCoord;
layout(location = 0) out vec4 color;

uniform sampler2D sourceTexture;
uniform float texelOffsetX;
uniform float texelOffsetY;

void main()
{
    vec2 texelDirection = vec2(texelOffsetX, texelOffsetY);
    vec3 result = texture(sourceTexture, fragTexCoord).rgb * 0.227027;
    result += texture(
        sourceTexture, fragTexCoord + texelDirection * 1.384615).rgb * 0.316216;
    result += texture(
        sourceTexture, fragTexCoord - texelDirection * 1.384615).rgb * 0.316216;
    result += texture(
        sourceTexture, fragTexCoord + texelDirection * 3.230769).rgb * 0.070270;
    result += texture(
        sourceTexture, fragTexCoord - texelDirection * 3.230769).rgb * 0.070270;
    color = vec4(result, 1.0);
}
