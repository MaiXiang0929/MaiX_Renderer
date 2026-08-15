// SPDX-License-Identifier: MIT
#pragma once

#include <array>

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

class ForwardPass;

/// Extracts and blurs HDR highlights before tone mapping.
class BloomPass final : public RenderPass
{
public:
    explicit BloomPass(ForwardPass& forwardPass)
        : m_ForwardPass(forwardPass)
    {
    }

    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override { return RenderPassType::Bloom; }
    void Execute(RenderPassContext& context) override;

    int GetTargetWidth() const { return m_Highlights.GetWidth(); }
    int GetTargetHeight() const { return m_Highlights.GetHeight(); }

private:
    static constexpr int BlurPassCount = 8;

    void BindTexture(GLuint texture, unsigned int unit) const;

    ForwardPass& m_ForwardPass;
    Shader m_ExtractShader;
    Shader m_BlurShader;
    Framebuffer m_Highlights;
    std::array<Framebuffer, 2> m_BlurTargets;
};
