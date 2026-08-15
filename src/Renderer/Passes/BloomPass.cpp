// SPDX-License-Identifier: MIT
#include "BloomPass.h"

#include "ForwardPass.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Resources/Mesh.h"

bool BloomPass::Init()
{
    return ReloadShaders();
}

bool BloomPass::Resize(unsigned int width, unsigned int height)
{
    const bool highlightsReady = m_Highlights.Init(
        static_cast<int>(width),
        static_cast<int>(height),
        FramebufferColorFormat::RGBA16F);
    const bool firstBlurReady = m_BlurTargets[0].Init(
        static_cast<int>(width),
        static_cast<int>(height),
        FramebufferColorFormat::RGBA16F);
    const bool secondBlurReady = m_BlurTargets[1].Init(
        static_cast<int>(width),
        static_cast<int>(height),
        FramebufferColorFormat::RGBA16F);
    return highlightsReady && firstBlurReady && secondBlurReady;
}

bool BloomPass::ReloadShaders()
{
    const bool extractLoaded = m_ExtractShader.Load(
        "assets/shaders/postprocess/fullscreen.vert",
        "assets/shaders/postprocess/bloom_extract.frag");
    const bool blurLoaded = m_BlurShader.Load(
        "assets/shaders/postprocess/fullscreen.vert",
        "assets/shaders/postprocess/bloom_blur.frag");
    return extractLoaded && blurLoaded;
}

void BloomPass::Execute(RenderPassContext& context)
{
    context.bloomTexture = 0;
    if (!context.postProcess.bloomEnabled)
        return;

    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_Highlights.Bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_ExtractShader.Bind();
    BindTexture(m_ForwardPass.GetColorTexture(), 0);
    m_ExtractShader.SetInt("sceneTexture", 0);
    m_ExtractShader.SetFloat(
        "threshold", context.postProcess.bloomThreshold);
    context.presentMesh.Draw();

    GLuint sourceTexture = m_Highlights.GetColorTexture();
    m_BlurShader.Bind();
    m_BlurShader.SetInt("sourceTexture", 0);
    const float inverseWidth = 1.0f / static_cast<float>(GetTargetWidth());
    const float inverseHeight = 1.0f / static_cast<float>(GetTargetHeight());
    for (int passIndex = 0; passIndex < BlurPassCount; ++passIndex)
    {
        Framebuffer& destination = m_BlurTargets[passIndex % 2];
        destination.Bind();
        BindTexture(sourceTexture, 0);
        const bool horizontal = passIndex % 2 == 0;
        m_BlurShader.SetFloat(
            "texelOffsetX", horizontal ? inverseWidth : 0.0f);
        m_BlurShader.SetFloat(
            "texelOffsetY", horizontal ? 0.0f : inverseHeight);
        context.presentMesh.Draw();
        sourceTexture = destination.GetColorTexture();
    }

    context.bloomTexture = sourceTexture;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND);
}

void BloomPass::BindTexture(GLuint texture, unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, unit, texture);
    glBindTexture(GL_TEXTURE_2D, texture);
}
