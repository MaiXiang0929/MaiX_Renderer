#pragma once

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"

class SSAOPass final : public RenderPass
{
public:
    explicit SSAOPass(RHIDevice& device) : m_Device(device), m_OcclusionShader(device), m_BlurShader(device), m_CompositeShader(device), m_Raw(device), m_Filtered(device), m_Composite(device) {}
    bool Init();
    bool Resize(unsigned int width, unsigned int height,
                unsigned int compositeWidth, unsigned int compositeHeight);
    bool ReloadShaders();
    RenderPassType GetType() const override { return RenderPassType::SSAO; }
    void Execute(RenderPassContext& context) override;
    int GetTargetWidth() const { return m_Raw.GetWidth(); }
    int GetTargetHeight() const { return m_Raw.GetHeight(); }
    int GetCompositeWidth() const { return m_Composite.GetWidth(); }
    int GetCompositeHeight() const { return m_Composite.GetHeight(); }
    bool TargetsMatch(int aoWidth, int aoHeight,
                      int compositeWidth, int compositeHeight) const;
    const Framebuffer& GetAoTarget() const { return m_Filtered; }
    const Framebuffer& GetTarget() const { return m_Composite; }

private:
    RHIDevice& m_Device;
    void BindTexture(RHITextureHandle texture, unsigned int unit) const;
    Shader m_OcclusionShader;
    Shader m_BlurShader;
    Shader m_CompositeShader;
    Framebuffer m_Raw;
    Framebuffer m_Filtered;
    Framebuffer m_Composite;
};
