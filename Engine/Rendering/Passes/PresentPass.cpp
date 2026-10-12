// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/PresentPass.h"

#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include "Engine/Rendering/RHI/OpenGL/OpenGLCommandList.h"

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
    const GLuint postProcessTexture = context.Resources().Texture(PassResourceId::PostColor);
    const GLuint editorOverlayTexture = context.Resources().Texture(PassResourceId::Overlay);

    // The existing resource table is bridged here; command execution is backend-owned.
    const OpenGLTextureView scene(postProcessTexture), overlay(editorOverlayTexture);
    OpenGLCommandList commands;
    commands.Composite(m_Shader, scene, overlay, context.presentMesh);
    context.Resources().EndTarget(PassResourceId::FinalColor);
    context.Resources().Publish(PassResourceId::FinalColor);
}
