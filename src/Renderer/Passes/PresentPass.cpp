// SPDX-License-Identifier: MIT
#include "PresentPass.h"

#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Resources/Mesh.h"

bool PresentPass::Init()
{
    return ReloadShaders();
}

bool PresentPass::ReloadShaders()
{
    return m_Shader.Load(
        "assets/shaders/present/present.vert",
        "assets/shaders/present/present.frag");
}

void PresentPass::Execute(RenderPassContext& context)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(
        0,
        0,
        static_cast<GLsizei>(context.frame.viewportWidth),
        static_cast<GLsizei>(context.frame.viewportHeight));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_Shader.Bind();
    m_Shader.SetMatrix4("mvp", &context.frame.presentMvp.cell[0]);
    glActiveTexture(GL_TEXTURE0);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 0, context.postProcessTexture);
    glBindTexture(GL_TEXTURE_2D, context.postProcessTexture);
    m_Shader.SetInt("renderedTexture", 0);
    context.presentMesh.Draw();
}
