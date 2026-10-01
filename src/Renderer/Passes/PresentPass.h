// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

/// @brief 将最终场景颜色与编辑器叠加层合成为视口可采样纹理。
class PresentPass final : public RenderPass
{
public:
    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();
    GLuint GetColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    const Framebuffer& GetTarget() const { return m_Framebuffer; }
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }

    RenderPassType GetType() const override
    {
        return RenderPassType::Present;
    }
    void Execute(RenderPassContext& context) override;

private:
    Shader m_Shader;
    Framebuffer m_Framebuffer;
};
