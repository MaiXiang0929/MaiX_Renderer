// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/PostProcessPass.h"

#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"

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
        "Shaders/postprocess/fullscreen.vert",
        "Shaders/postprocess/postprocess.frag");
}

void PostProcessPass::Execute(RenderPassContext& context)
{
    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    context.Resources().BeginTarget(PassResourceId::PostColor);
    m_Shader.Bind();

    glActiveTexture(GL_TEXTURE0);
    const GLuint sceneColorTexture = context.Resources().HdrTexture();
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 0, sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    m_Shader.SetInt("sceneTexture", 0);

    const bool bloomEnabled = context.postProcess.bloomEnabled;
    if (bloomEnabled)
    {
        const GLuint bloomTexture = context.Resources().Texture(PassResourceId::Bloom);
        glActiveTexture(GL_TEXTURE1);
        RenderSubmissionStats::Get().RecordTextureBind(
            GL_TEXTURE_2D, 1, bloomTexture);
        glBindTexture(GL_TEXTURE_2D, bloomTexture);
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

    context.Resources().EndTarget(PassResourceId::PostColor);
    context.Resources().Publish(PassResourceId::PostColor);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND);
}
