// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"

/// @brief 将最终场景颜色与编辑器叠加层合成为视口可采样纹理。
class PresentPass final : public RenderPass
{
public:
    explicit PresentPass(RHIDevice& device) : m_Device(device), m_Shader(device), m_Framebuffer(device) {}
    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();
    RHITextureHandle GetColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    const Framebuffer& GetTarget() const { return m_Framebuffer; }
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }

    RenderPassType GetType() const override
    {
        return RenderPassType::Present;
    }
    void Execute(RenderPassContext& context) override;

private:
    RHIDevice& m_Device;
    Shader m_Shader;
    Framebuffer m_Framebuffer;
};
