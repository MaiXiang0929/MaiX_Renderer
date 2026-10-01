// SPDX-License-Identifier: MIT
#include "ReflectionPass.h"

#include "ForwardPass.h"
#include "TranslucencyPass.h"

bool ReflectionPass::Init()
{
    return true;
}

bool ReflectionPass::Resize(unsigned int width, unsigned int height)
{
    return m_Framebuffer.Init(
        static_cast<int>(width),
        static_cast<int>(height),
        FramebufferColorFormat::RGBA8);
}

bool ReflectionPass::ReloadShaders()
{
    return true;
}

void ReflectionPass::Execute(RenderPassContext& context)
{
    context.Resources().BeginTarget(PassResourceId::ReflectionColor);

    m_ForwardPass.RenderSkybox(context, context.frame.reflectionView);
    m_ForwardPass.RenderSurface(context, context.reflectionView);
    m_TranslucencyPass.RenderToBoundTarget(
        context, context.reflectionView);

    context.Resources().EndTarget(PassResourceId::ReflectionColor);
    context.Resources().Publish(PassResourceId::ReflectionColor);
}
