// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"
class ShadowPreviewPass final : public RenderPass
{
public:
    explicit ShadowPreviewPass(RHIDevice& device) : m_Device(device), m_Shader(device), m_Target(device) {}
    bool Init() { return ReloadShaders(); }
    bool ReloadShaders() { return m_Shader.Load("Shaders/present/present.vert", "Shaders/shadow/preview.frag"); }
    bool Prepare();
    RenderPassType GetType() const override { return RenderPassType::ShadowPreview; }
    void Execute(RenderPassContext& context) override;
    const Framebuffer& GetTarget() const { return m_Target; }
private:
    RHIDevice& m_Device;
    Shader m_Shader;
    Framebuffer m_Target;
};
