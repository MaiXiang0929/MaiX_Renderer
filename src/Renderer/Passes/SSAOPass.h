#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

class ForwardPass;

class SSAOPass final : public RenderPass
{
public:
    explicit SSAOPass(ForwardPass& forwardPass) : m_ForwardPass(forwardPass) {}
    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();
    RenderPassType GetType() const override { return RenderPassType::SSAO; }
    void Execute(RenderPassContext& context) override;
    int GetTargetWidth() const { return m_Raw.GetWidth(); }
    int GetTargetHeight() const { return m_Raw.GetHeight(); }
    int GetCompositeWidth() const { return m_Composite.GetWidth(); }
    int GetCompositeHeight() const { return m_Composite.GetHeight(); }

private:
    void BindTexture(GLuint texture, unsigned int unit) const;
    ForwardPass& m_ForwardPass;
    Shader m_OcclusionShader;
    Shader m_BlurShader;
    Shader m_CompositeShader;
    Framebuffer m_Raw;
    Framebuffer m_Filtered;
    Framebuffer m_Composite;
};
