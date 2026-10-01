#include "SSAOPass.h"

#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Resources/Mesh.h"
#include "Renderer/View/RenderView.h"

bool SSAOPass::Init() { return ReloadShaders(); }

bool SSAOPass::Resize(unsigned int width, unsigned int height,
                      unsigned int compositeWidth, unsigned int compositeHeight)
{
    FramebufferSpecification ao;
    ao.width = static_cast<int>(width);
    ao.height = static_cast<int>(height);
    ao.colorFormat = FramebufferColorFormat::R8;
    ao.depthStencilEnabled = false;
    ao.mipmapsEnabled = false;
    FramebufferSpecification composite = ao;
    composite.width = static_cast<int>(compositeWidth);
    composite.height = static_cast<int>(compositeHeight);
    composite.colorFormat = FramebufferColorFormat::RGBA16F;
    return m_Raw.Init(ao) && m_Filtered.Init(ao) && m_Composite.Init(composite);
}

bool SSAOPass::TargetsMatch(int aoWidth, int aoHeight,
                            int compositeWidth, int compositeHeight) const
{
    return m_Raw.GetWidth() == aoWidth &&
        m_Raw.GetHeight() == aoHeight &&
        m_Filtered.GetWidth() == aoWidth &&
        m_Filtered.GetHeight() == aoHeight &&
        m_Composite.GetWidth() == compositeWidth &&
        m_Composite.GetHeight() == compositeHeight;
}

bool SSAOPass::ReloadShaders()
{
    return m_OcclusionShader.Load("assets/shaders/postprocess/fullscreen.vert", "assets/shaders/postprocess/ssao.frag") &&
        m_BlurShader.Load("assets/shaders/postprocess/fullscreen.vert", "assets/shaders/postprocess/ssao_blur.frag") &&
        m_CompositeShader.Load("assets/shaders/postprocess/fullscreen.vert", "assets/shaders/postprocess/ssao_composite.frag");
}

void SSAOPass::BindTexture(GLuint texture, unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    RenderSubmissionStats::Get().RecordTextureBind(GL_TEXTURE_2D, unit, texture);
    glBindTexture(GL_TEXTURE_2D, texture);
}

void SSAOPass::Execute(RenderPassContext& context)
{
    if (!context.postProcess.ssaoEnabled) return;
    const auto& depth = context.Resources().Read(PassResourceId::ForwardDepth);
    const GLuint sceneColor = context.Resources().Texture(PassResourceId::SceneHdrColor);

    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    const cy::Matrix4f inverseProjection = context.mainView.projection.GetInverse();
    const float invW = 1.0f / static_cast<float>(depth.width);
    const float invH = 1.0f / static_cast<float>(depth.height);

    m_Raw.Bind(); glClear(GL_COLOR_BUFFER_BIT); m_OcclusionShader.Bind();
    BindTexture(depth.texture, 0);
    m_OcclusionShader.SetInt("depthTexture", 0);
    m_OcclusionShader.SetMatrix4("inverseProjection", &inverseProjection.cell[0]);
    m_OcclusionShader.SetVec3("depthTexelSize", invW, invH, 0.0f);
    m_OcclusionShader.SetFloat("sampleRadius", context.postProcess.ssaoRadius);
    m_OcclusionShader.SetFloat("bias", context.postProcess.ssaoBias);
    context.presentMesh.Draw(); m_Raw.Unbind();

    context.Resources().BeginTarget(PassResourceId::SsaoAO); m_BlurShader.Bind();
    BindTexture(m_Raw.GetColorTexture(), 0); BindTexture(depth.texture, 1);
    m_BlurShader.SetInt("aoTexture", 0); m_BlurShader.SetInt("depthTexture", 1);
    m_BlurShader.SetVec3("depthTexelSize", invW, invH, 0.0f);
    context.presentMesh.Draw(); context.Resources().EndTarget(PassResourceId::SsaoAO);

    context.Resources().BeginTarget(PassResourceId::SsaoColor); m_CompositeShader.Bind();
    BindTexture(sceneColor, 0); BindTexture(m_Filtered.GetColorTexture(), 1);
    m_CompositeShader.SetInt("sceneTexture", 0); m_CompositeShader.SetInt("aoTexture", 1);
    m_CompositeShader.SetFloat("intensity", context.postProcess.ssaoIntensity);
    context.presentMesh.Draw(); context.Resources().EndTarget(PassResourceId::SsaoColor);
    context.Resources().Publish(PassResourceId::SsaoAO);
    context.Resources().Publish(PassResourceId::SsaoColor);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND);
}
