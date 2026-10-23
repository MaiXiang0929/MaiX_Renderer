// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>

struct EnvironmentSettings
{
    float intensity = 1.0f;
    float rotationDegrees = 0.0f;
};
inline bool IsValidEnvironmentSettings(const EnvironmentSettings& settings)
{
    return std::isfinite(settings.intensity) && settings.intensity >= 0 && settings.intensity <= 16 &&
        std::isfinite(settings.rotationDegrees) && settings.rotationDegrees >= -180 && settings.rotationDegrees <= 180;
}

struct TessellationSettings
{
    bool enabled = false;
    float level = 8.0f;
    float displacementScale = 0.35f;
    bool wireframe = false;
};

struct PostProcessSettings
{
    bool fxaaEnabled = true;
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

constexpr float MinimumSsaoRadius = 0.001f;
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

// Shared by scene files and frame submission; invalid authored values are rejected,
// never silently clamped while loading a scene.
inline bool IsValidPostProcessSettings(const PostProcessSettings& settings)
{
    const auto within = [](float value, float minimum, float maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    };
    return within(settings.exposureCompensation, MinimumExposureCompensation, MaximumExposureCompensation) &&
        within(settings.bloomThreshold, MinimumBloomThreshold, MaximumBloomThreshold) &&
        within(settings.bloomIntensity, MinimumBloomIntensity, MaximumBloomIntensity) &&
        within(settings.ssaoRadius, MinimumSsaoRadius, MaximumSsaoRadius) &&
        within(settings.ssaoIntensity, MinimumSsaoIntensity, MaximumSsaoIntensity) &&
        within(settings.ssaoBias, MinimumSsaoBias, MaximumSsaoBias);
}
