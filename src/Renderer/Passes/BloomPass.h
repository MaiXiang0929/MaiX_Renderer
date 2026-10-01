// SPDX-License-Identifier: MIT
#pragma once

#include <array>

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

/// Extracts and blurs HDR highlights before tone mapping.
class BloomPass final : public RenderPass
{
public:
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
    static constexpr int BlurPassCount = 8;

    void BindTexture(GLuint texture, unsigned int unit) const;

    Shader m_ExtractShader;
    Shader m_BlurShader;
    Framebuffer m_Highlights;
    std::array<Framebuffer, 2> m_BlurTargets;
};
