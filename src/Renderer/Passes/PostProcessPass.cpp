// SPDX-License-Identifier: MIT
#include "PostProcessPass.h"

#include "ForwardPass.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Resources/Mesh.h"

bool PostProcessPass::Init()
{
    return ReloadShaders();
}

bool PostProcessPass::Resize(unsigned int width, unsigned int height)
{
    return m_Framebuffer.Init(
        static_cast<int>(width),
        static_cast<int>(height),
        FramebufferColorFormat::RGBA8);
}

bool PostProcessPass::ReloadShaders()
{
    return m_Shader.Load(
        "assets/shaders/postprocess/fullscreen.vert",
        "assets/shaders/postprocess/postprocess.frag");
}

void PostProcessPass::Execute(RenderPassContext& context)
{
    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_Framebuffer.Bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_Shader.Bind();

    glActiveTexture(GL_TEXTURE0);
    const GLuint sceneColorTexture = context.sceneColorTexture != 0
        ? context.sceneColorTexture
        : m_ForwardPass.GetColorTexture();
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 0, sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    m_Shader.SetInt("sceneTexture", 0);

    const bool bloomEnabled = context.postProcess.bloomEnabled &&
        context.bloomTexture != 0;
    if (bloomEnabled)
    {
        glActiveTexture(GL_TEXTURE1);
        RenderSubmissionStats::Get().RecordTextureBind(
            GL_TEXTURE_2D, 1, context.bloomTexture);
        glBindTexture(GL_TEXTURE_2D, context.bloomTexture);
    }
    m_Shader.SetInt("bloomTexture", 1);
    m_Shader.SetInt("bloomEnabled", bloomEnabled ? 1 : 0);
    m_Shader.SetFloat("bloomIntensity", context.postProcess.bloomIntensity);
    m_Shader.SetInt(
        "toneMappingEnabled",
        context.postProcess.toneMappingEnabled ? 1 : 0);
    m_Shader.SetFloat(
        "exposureCompensation",
        context.postProcess.exposureCompensation);
    context.presentMesh.Draw();

    m_Framebuffer.Unbind();
    m_Framebuffer.GenerateMipmaps();
    context.postProcessTexture = GetColorTexture();
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND);
}
