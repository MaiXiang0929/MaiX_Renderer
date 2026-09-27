// SPDX-License-Identifier: MIT
#include "PresentPass.h"

#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Resources/Mesh.h"

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
        "assets/shaders/present/present.vert",
        "assets/shaders/present/present.frag");
}

void PresentPass::Execute(RenderPassContext& context)
{
    // 最终颜色由 PresentPass 持有；ImGui 仅在同一帧读取纹理，不管理其生命周期。
    m_Framebuffer.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_Shader.Bind();
    glActiveTexture(GL_TEXTURE0);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 0, context.postProcessTexture);
    glBindTexture(GL_TEXTURE_2D, context.postProcessTexture);
    m_Shader.SetInt("renderedTexture", 0);
    glActiveTexture(GL_TEXTURE1);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 1, context.editorOverlayTexture);
    glBindTexture(GL_TEXTURE_2D, context.editorOverlayTexture);
    m_Shader.SetInt("editorOverlayTexture", 1);
    context.presentMesh.Draw();
    m_Framebuffer.Unbind();
}
