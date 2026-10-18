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
    RHIScopedPipelineState stateScope(m_Device.Commands());
    auto state=stateScope.saved; state.depthTest=false; state.blend=false; state.cull=RHICull::None;
    m_Device.Commands().SetPipelineState(state);

    context.Resources().BeginTarget(PassResourceId::PostColor);
    m_Shader.Bind();

    const RHITextureHandle sceneColorTexture = context.Resources().HdrTexture();
    m_Device.Commands().BindTexture(sceneColorTexture, 0);
    m_Shader.SetInt("sceneTexture", 0);

    const bool bloomEnabled = context.postProcess.bloomEnabled;
    if (bloomEnabled)
    {
        const RHITextureHandle bloomTexture = context.Resources().Texture(PassResourceId::Bloom);
        m_Device.Commands().BindTexture(bloomTexture, 1);
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

}
