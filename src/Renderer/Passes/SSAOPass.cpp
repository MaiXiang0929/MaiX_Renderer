#include "SSAOPass.h"

#include "ForwardPass.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Resources/Mesh.h"
#include "Renderer/View/RenderView.h"

bool SSAOPass::Init() { return ReloadShaders(); }

bool SSAOPass::Resize(unsigned int width, unsigned int height)
{
    FramebufferSpecification ao;
    ao.width = static_cast<int>(width);
    ao.height = static_cast<int>(height);
    ao.colorFormat = FramebufferColorFormat::R8;
    ao.depthStencilEnabled = false;
    ao.mipmapsEnabled = false;
    FramebufferSpecification composite = ao;
    composite.width = m_ForwardPass.GetTargetWidth();
    composite.height = m_ForwardPass.GetTargetHeight();
    composite.colorFormat = FramebufferColorFormat::RGBA16F;
    return m_Raw.Init(ao) && m_Filtered.Init(ao) && m_Composite.Init(composite);
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
    context.sceneColorTexture = m_ForwardPass.GetColorTexture();
    context.ssaoTexture = 0;
    if (!context.postProcess.ssaoEnabled || m_ForwardPass.GetDepthTexture() == 0 ||
        m_Raw.GetColorTexture() == 0 || m_Composite.GetColorTexture() == 0) return;

    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    const cy::Matrix4f inverseProjection = context.mainView.projection.GetInverse();
    const float invW = 1.0f / static_cast<float>(m_ForwardPass.GetTargetWidth());
    const float invH = 1.0f / static_cast<float>(m_ForwardPass.GetTargetHeight());

    m_Raw.Bind(); glClear(GL_COLOR_BUFFER_BIT); m_OcclusionShader.Bind();
    BindTexture(m_ForwardPass.GetDepthTexture(), 0);
    m_OcclusionShader.SetInt("depthTexture", 0);
    m_OcclusionShader.SetMatrix4("inverseProjection", &inverseProjection.cell[0]);
    m_OcclusionShader.SetVec3("depthTexelSize", invW, invH, 0.0f);
    m_OcclusionShader.SetFloat("sampleRadius", context.postProcess.ssaoRadius);
    m_OcclusionShader.SetFloat("bias", context.postProcess.ssaoBias);
    context.presentMesh.Draw(); m_Raw.Unbind();

    m_Filtered.Bind(); glClear(GL_COLOR_BUFFER_BIT); m_BlurShader.Bind();
    BindTexture(m_Raw.GetColorTexture(), 0); BindTexture(m_ForwardPass.GetDepthTexture(), 1);
    m_BlurShader.SetInt("aoTexture", 0); m_BlurShader.SetInt("depthTexture", 1);
    m_BlurShader.SetVec3("depthTexelSize", invW, invH, 0.0f);
    context.presentMesh.Draw(); m_Filtered.Unbind();

    m_Composite.Bind(); glClear(GL_COLOR_BUFFER_BIT); m_CompositeShader.Bind();
    BindTexture(m_ForwardPass.GetColorTexture(), 0); BindTexture(m_Filtered.GetColorTexture(), 1);
    m_CompositeShader.SetInt("sceneTexture", 0); m_CompositeShader.SetInt("aoTexture", 1);
    m_CompositeShader.SetFloat("intensity", context.postProcess.ssaoIntensity);
    context.presentMesh.Draw(); m_Composite.Unbind();
    context.sceneColorTexture = m_Composite.GetColorTexture();
    context.ssaoTexture = m_Filtered.GetColorTexture();
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND);
}
