// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

class ForwardPass;

/// Composites HDR post effects, applies exposure and maps the result to sRGB.
class PostProcessPass final : public RenderPass
{
public:
    explicit PostProcessPass(ForwardPass& forwardPass)
        : m_ForwardPass(forwardPass)
    {
    }

    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::PostProcess;
    }
    void Execute(RenderPassContext& context) override;

    GLuint GetColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }

private:
    ForwardPass& m_ForwardPass;
    Shader m_Shader;
    Framebuffer m_Framebuffer;
};
