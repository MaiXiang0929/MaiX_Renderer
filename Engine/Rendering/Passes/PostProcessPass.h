// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"

/// Composites HDR post effects, applies exposure and maps the result to sRGB.
class PostProcessPass final : public RenderPass
{
public:
    explicit PostProcessPass(RHIDevice& device) : m_Device(device), m_Shader(device), m_Framebuffer(device) {}
    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::PostProcess;
    }
    void Execute(RenderPassContext& context) override;

    RHITextureHandle GetColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    const Framebuffer& GetTarget() const { return m_Framebuffer; }
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }

private:
    RHIDevice& m_Device;
    Shader m_Shader;
    Framebuffer m_Framebuffer;
};
