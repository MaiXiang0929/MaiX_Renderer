// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Pipeline/RenderPass.h"

class ForwardPass;
class TranslucencyPass;

/// @brief 反射 Pass：用镜像视图重绘天空盒和场景模型 到独立纹理。
class ReflectionPass final : public RenderPass
{
public:
    ReflectionPass(
        ForwardPass& forwardPass,
        TranslucencyPass& translucencyPass)
        : m_ForwardPass(forwardPass)
        , m_TranslucencyPass(translucencyPass)
    {
    }

    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }
    bool ReloadShaders();
    const Framebuffer& GetTarget() const { return m_Framebuffer; }

    RenderPassType GetType() const override
    {
        return RenderPassType::Reflection;
    }
    void Execute(RenderPassContext& context) override;

private:
    ForwardPass& m_ForwardPass;
    TranslucencyPass& m_TranslucencyPass;
    Framebuffer m_Framebuffer;
};
