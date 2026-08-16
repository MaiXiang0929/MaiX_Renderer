// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>

#include "Renderer/Pipeline/RenderSettings.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "[PostProcessSettingsTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}
}

int main()
{
    const PostProcessSettings defaults;
    Require(!defaults.ssaoEnabled,
        "SSAO should remain opt-in until the artist enables it.");
    Require(defaults.ssaoRadius == 0.5f && defaults.ssaoIntensity == 1.0f,
        "SSAO defaults should provide a restrained artist-facing baseline.");
    Require(defaults.bloomEnabled,
        "Bloom should be enabled by default.");
    Require(defaults.bloomThreshold == 1.0f,
        "Default bloom threshold should select HDR highlights.");
    Require(defaults.bloomIntensity == 0.35f,
        "Default bloom intensity should remain restrained.");
    Require(defaults.toneMappingEnabled,
        "Tone mapping should be enabled by default.");
    Require(defaults.exposureCompensation == 0.0f,
        "Default exposure should preserve scene luminance.");
    Require(ClampExposureCompensation(-20.0f) ==
        MinimumExposureCompensation,
        "Exposure should clamp to its lower artist-facing limit.");
    Require(ClampExposureCompensation(20.0f) ==
        MaximumExposureCompensation,
        "Exposure should clamp to its upper artist-facing limit.");
    Require(ClampExposureCompensation(1.5f) == 1.5f,
        "Exposure inside the supported range should remain unchanged.");
    Require(ClampBloomThreshold(-1.0f) == MinimumBloomThreshold,
        "Bloom threshold should clamp to its lower limit.");
    Require(ClampBloomThreshold(20.0f) == MaximumBloomThreshold,
        "Bloom threshold should clamp to its upper limit.");
    Require(ClampBloomIntensity(-1.0f) == MinimumBloomIntensity,
        "Bloom intensity should clamp to its lower limit.");
    Require(ClampBloomIntensity(8.0f) == MaximumBloomIntensity,
        "Bloom intensity should clamp to its upper limit.");
    Require(ClampSsaoRadius(-1.0f) == MinimumSsaoRadius &&
        ClampSsaoRadius(8.0f) == MaximumSsaoRadius,
        "SSAO radius should clamp to its supported range.");
    Require(ClampSsaoIntensity(-1.0f) == MinimumSsaoIntensity &&
        ClampSsaoIntensity(8.0f) == MaximumSsaoIntensity,
        "SSAO intensity should clamp to its supported range.");
    Require(ClampSsaoBias(-1.0f) == MinimumSsaoBias &&
        ClampSsaoBias(8.0f) == MaximumSsaoBias,
        "SSAO bias should clamp to its supported range.");

    std::cout << "PostProcessSettingsTests passed." << std::endl;
    return EXIT_SUCCESS;
}
