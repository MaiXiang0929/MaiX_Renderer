// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/PresentPass.h"

#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"


bool PresentPass::Init()
{
    return ReloadShaders();
}

bool PresentPass::Resize(unsigned int width, unsigned int height)
{
    FramebufferSpecification specification;
    specification.width = static_cast<int>(width);
    specification.height = static_cast<int>(height);
    specification.colorFormat = FramebufferColorFormat::RGBA8;
    specification.depthStencilEnabled = false;
    specification.mipmapsEnabled = false;
    return m_Framebuffer.Init(specification);
}

bool PresentPass::ReloadShaders()
{
    return m_Shader.Load(
        "Shaders/present/present.vert",
        "Shaders/present/present.frag");
}

void PresentPass::Execute(RenderPassContext& context)
{
    // 最终颜色由 PresentPass 持有；ImGui 仅在同一帧读取纹理，不管理其生命周期。
    context.Resources().BeginTarget(PassResourceId::FinalColor);
    const RHITextureHandle postProcessTexture = context.Resources().Texture(PassResourceId::PostColor);
    const RHITextureHandle editorOverlayTexture = context.Resources().Texture(PassResourceId::Overlay);

    RHIScopedPipelineState stateScope(m_Device.Commands());
    auto state=stateScope.saved; state.depthTest=false; state.depthWrite=false; state.blend=false; state.cull=RHICull::None;
    m_Device.Commands().SetPipelineState(state);
    m_Shader.Bind();
    m_Device.Commands().BindTexture(postProcessTexture,0);
    m_Device.Commands().BindTexture(editorOverlayTexture,1);
    m_Shader.SetInt("fxaaEnabled", context.postProcess.fxaaEnabled ? 1 : 0);
    m_Shader.SetInt("renderedTexture",0); m_Shader.SetInt("editorOverlayTexture",1);
    context.presentMesh.Draw();
    context.Resources().EndTarget(PassResourceId::FinalColor);
    context.Resources().Publish(PassResourceId::FinalColor);
}
