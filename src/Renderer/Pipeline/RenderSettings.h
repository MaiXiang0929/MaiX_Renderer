// SPDX-License-Identifier: MIT
#pragma once

struct TessellationSettings
{
    bool enabled = false;
    float level = 8.0f;
    float displacementScale = 0.35f;
    bool wireframe = false;
};

struct PostProcessSettings
{
    bool ssaoEnabled = false;
    float ssaoRadius = 0.5f;
    float ssaoIntensity = 1.0f;
    float ssaoBias = 0.025f;
    bool bloomEnabled = true;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.35f;
    bool toneMappingEnabled = true;
    float exposureCompensation = 0.0f;
};

constexpr float MinimumSsaoRadius = 0.05f;
constexpr float MaximumSsaoRadius = 2.0f;
constexpr float MinimumSsaoIntensity = 0.0f;
constexpr float MaximumSsaoIntensity = 3.0f;
constexpr float MinimumSsaoBias = 0.0f;
constexpr float MaximumSsaoBias = 0.2f;

constexpr float ClampSsaoRadius(float radius)
{
    return radius < MinimumSsaoRadius ? MinimumSsaoRadius
        : radius > MaximumSsaoRadius ? MaximumSsaoRadius : radius;
}

constexpr float ClampSsaoIntensity(float intensity)
{
    return intensity < MinimumSsaoIntensity ? MinimumSsaoIntensity
        : intensity > MaximumSsaoIntensity ? MaximumSsaoIntensity : intensity;
}

constexpr float ClampSsaoBias(float bias)
{
    return bias < MinimumSsaoBias ? MinimumSsaoBias
        : bias > MaximumSsaoBias ? MaximumSsaoBias : bias;
}

constexpr float MinimumBloomThreshold = 0.0f;
constexpr float MaximumBloomThreshold = 10.0f;
constexpr float MinimumBloomIntensity = 0.0f;
constexpr float MaximumBloomIntensity = 4.0f;

constexpr float MinimumExposureCompensation = -8.0f;
constexpr float MaximumExposureCompensation = 8.0f;

constexpr float ClampExposureCompensation(float exposure)
{
    return exposure < MinimumExposureCompensation
        ? MinimumExposureCompensation
        : exposure > MaximumExposureCompensation
            ? MaximumExposureCompensation
            : exposure;
}

constexpr float ClampBloomThreshold(float threshold)
{
    return threshold < MinimumBloomThreshold
        ? MinimumBloomThreshold
        : threshold > MaximumBloomThreshold
            ? MaximumBloomThreshold
            : threshold;
}

constexpr float ClampBloomIntensity(float intensity)
{
    return intensity < MinimumBloomIntensity
        ? MinimumBloomIntensity
        : intensity > MaximumBloomIntensity
            ? MaximumBloomIntensity
            : intensity;
}
