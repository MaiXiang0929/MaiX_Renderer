// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

/// Composites HDR post effects, applies exposure and maps the result to sRGB.
class PostProcessPass final : public RenderPass
{
public:
    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::PostProcess;
    }
    void Execute(RenderPassContext& context) override;

    GLuint GetColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    const Framebuffer& GetTarget() const { return m_Framebuffer; }
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }

private:
    Shader m_Shader;
    Framebuffer m_Framebuffer;
};
