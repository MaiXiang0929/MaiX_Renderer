// SPDX-License-Identifier: MIT
#include "Engine/Rendering/RHI/OpenGL/OpenGLCommandList.h"
#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include <glad/glad.h>

void OpenGLTextureView::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    RenderSubmissionStats::Get().RecordTextureBind(GL_TEXTURE_2D, slot, m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
}

void OpenGLCommandList::Composite(RHIPipeline& pipeline, const RHITextureView& scene,
    const RHITextureView& overlay, const RHIBuffer& geometry)
{
    pipeline.Bind();
    scene.Bind(0);
    pipeline.SetInt("renderedTexture", 0);
    overlay.Bind(1);
    pipeline.SetInt("editorOverlayTexture", 1);
    geometry.Draw();
}
