#version 330 core
in vec2 fragTexCoord;
layout(location = 0) out float color;
uniform sampler2D aoTexture;
uniform sampler2D depthTexture;
uniform vec3 depthTexelSize;
void main()
{
    float centerDepth = texture(depthTexture, fragTexCoord).r;
    if (centerDepth >= 0.99999) { color = 1.0; return; }
    float sum = 0.0; float weightSum = 0.0;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
    {
        vec2 uv = fragTexCoord + vec2(x, y) * depthTexelSize.xy * 2.0;
        float sampleDepth = texture(depthTexture, uv).r;
        float weight = abs(sampleDepth - centerDepth) < 0.01 ? 1.0 : 0.0;
        sum += texture(aoTexture, uv).r * weight;
        weightSum += weight;
    }
    color = weightSum > 0.0 ? sum / weightSum : texture(aoTexture, fragTexCoord).r;
}
