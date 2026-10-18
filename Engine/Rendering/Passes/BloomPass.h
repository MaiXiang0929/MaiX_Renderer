// SPDX-License-Identifier: MIT
#pragma once

#include <array>

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"

/// Extracts and blurs HDR highlights before tone mapping.
class BloomPass final : public RenderPass
{
public:
    explicit BloomPass(RHIDevice& device) : m_Device(device), m_ExtractShader(device), m_BlurShader(device), m_Highlights(device), m_BlurTargets{{Framebuffer(device), Framebuffer(device)}} {}
    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override { return RenderPassType::Bloom; }
    void Execute(RenderPassContext& context) override;

    int GetTargetWidth() const { return m_Highlights.GetWidth(); }
    int GetTargetHeight() const { return m_Highlights.GetHeight(); }
    bool TargetsMatch(int width, int height) const;
    const Framebuffer& GetTarget() const { return m_BlurTargets[(BlurPassCount - 1) % 2]; }

private:
    RHIDevice& m_Device;
    static constexpr int BlurPassCount = 8;

    void BindTexture(RHITextureHandle texture, unsigned int unit) const;

    Shader m_ExtractShader;
    Shader m_BlurShader;
    Framebuffer m_Highlights;
    std::array<Framebuffer, 2> m_BlurTargets;
};
