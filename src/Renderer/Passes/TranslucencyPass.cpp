// SPDX-License-Identifier: MIT
#include "TranslucencyPass.h"

#include "ForwardPass.h"
#include "Renderer/View/RenderView.h"

void TranslucencyPass::Execute(RenderPassContext& context)
{
    context.Resources().BeginTarget(PassResourceId::SceneHdrColor);
    RenderToBoundTarget(context, context.mainView);
    context.Resources().EndTarget(PassResourceId::SceneHdrColor);
    context.Resources().Publish(PassResourceId::SceneHdrColor);
}

void TranslucencyPass::RenderToBoundTarget(
    RenderPassContext& context,
    RenderView& view)
{
    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthWriteEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);

    GLint blendSourceRgb = GL_ONE;
    GLint blendDestinationRgb = GL_ZERO;
    GLint blendSourceAlpha = GL_ONE;
    GLint blendDestinationAlpha = GL_ZERO;
    GLint blendEquationRgb = GL_FUNC_ADD;
    GLint blendEquationAlpha = GL_FUNC_ADD;
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);

    // Preserve opaque depth for testing but do not let translucent layers occlude each other.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_ForwardPass.RenderTranslucentSurface(context, view);

    if (!blendEnabled)
        glDisable(GL_BLEND);
    glBlendFuncSeparate(
        blendSourceRgb,
        blendDestinationRgb,
        blendSourceAlpha,
        blendDestinationAlpha);
    glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
    if (!depthTestEnabled)
        glDisable(GL_DEPTH_TEST);
    glDepthMask(depthWriteEnabled);
}
