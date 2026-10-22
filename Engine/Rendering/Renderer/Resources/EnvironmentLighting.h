// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/RHI/RHIDevice.h"

// Renderer-owned derived environment textures. Build only between frames.
// All three resources commit together; failure preserves the old lighting.
class EnvironmentLighting
{
public:
    static constexpr unsigned IrradianceSize = 32, PrefilterSize = 128, BrdfSize = 256;
    static constexpr unsigned PrefilterMaxMip = 7;
    explicit EnvironmentLighting(RHIDevice& device) : m_Device(device) {}
    ~EnvironmentLighting();
    EnvironmentLighting(const EnvironmentLighting&) = delete;
    EnvironmentLighting& operator=(const EnvironmentLighting&) = delete;
    bool Build(RHITextureHandle environment, unsigned sourceResolution);
    RHITextureHandle Irradiance() const { return m_Irradiance; }
    RHITextureHandle Prefiltered() const { return m_Prefiltered; }
    RHITextureHandle Brdf() const { return m_Brdf.color; }
private:
    RHIDevice& m_Device;
    RHITextureHandle m_Irradiance, m_Prefiltered;
    RHITarget m_Brdf;
};
