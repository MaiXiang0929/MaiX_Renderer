#version 330 core

in vec2 fragTexCoord;
layout(location = 0) out vec4 color;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform bool bloomEnabled;
uniform float bloomIntensity;
uniform bool toneMappingEnabled;
uniform float exposureCompensation;

vec3 AcesFitted(vec3 colorValue)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp(
        (colorValue * (a * colorValue + b)) /
        (colorValue * (c * colorValue + d) + e),
        0.0,
        1.0);
}

vec3 LinearToSrgb(vec3 linearColor)
{
    vec3 low = linearColor * 12.92;
    vec3 high = 1.055 * pow(
        max(linearColor, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(linearColor, vec3(0.0031308)));
}

void main()
{
    vec3 sceneColor = max(texture(sceneTexture, fragTexCoord).rgb, vec3(0.0));
    if (bloomEnabled)
    {
        sceneColor += max(
            texture(bloomTexture, fragTexCoord).rgb, vec3(0.0)) *
            bloomIntensity;
    }
    vec3 exposedColor = sceneColor * exp2(exposureCompensation);
    vec3 displayLinear = toneMappingEnabled
        ? AcesFitted(exposedColor)
        : clamp(exposedColor, 0.0, 1.0);
    color = vec4(LinearToSrgb(displayLinear), 1.0);
}
