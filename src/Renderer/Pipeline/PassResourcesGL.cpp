// SPDX-License-Identifier: MIT
#include "PassResourceContract.h"

#include <glad/glad.h>

void PassResources::BeginTarget(PassResourceId id) const
{
    const auto& binding = Output(id);
    const auto& declaration = FindOutput(id);
    glBindFramebuffer(GL_FRAMEBUFFER, binding.framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(binding.width), static_cast<GLsizei>(binding.height));
    const bool clears = declaration.colorLoad == AttachmentLoad::Clear ||
        declaration.depthLoad == AttachmentLoad::Clear;
    const GLboolean scissor = clears ? glIsEnabled(GL_SCISSOR_TEST) : GL_FALSE;
    if (scissor) glDisable(GL_SCISSOR_TEST);
    // Clear 不受上一个 Pass 遗留的写掩码影响；保留行为完全不发出清空命令。
    if (declaration.colorLoad == AttachmentLoad::Clear)
    {
        GLboolean mask[4];
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, id == PassResourceId::Overlay ? 0.0f : 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
    }
    if (declaration.depthLoad == AttachmentLoad::Clear)
    {
        GLboolean mask;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDepthMask(mask);
    }
    if (scissor) glEnable(GL_SCISSOR_TEST);
}

void PassResources::EndTarget(PassResourceId id, bool generateMipmaps) const
{
    const auto& binding = Output(id);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (generateMipmaps && binding.mipmaps)
    {
        glBindTexture(GL_TEXTURE_2D, binding.texture);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
