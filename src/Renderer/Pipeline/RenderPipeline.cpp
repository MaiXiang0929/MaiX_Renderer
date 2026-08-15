// SPDX-License-Identifier: MIT
/// @file RenderPipeline.cpp
/// @brief 渲染管线类的实现文件
/// @details 该文件实现了 RenderPipeline 类的核心功能，包括初始化各个渲染阶段、执行渲染流程等。
/// @author MaiX
/// @date 2026-08-11


#include "RenderPipeline.h"

#include <iostream>

#include "Renderer/Diagnostics/GpuDebugScope.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Core/OpenGLStateCache.h"
#include "Renderer/Pipeline/RenderTargetSizing.h"

namespace
{
const char* GetPassDebugName(RenderPassType type)
{
    switch (type)
    {
    case RenderPassType::Shadow: return "MaiX.ShadowPass";
    case RenderPassType::Reflection: return "MaiX.ReflectionPass";
    case RenderPassType::Forward: return "MaiX.ForwardPass";
    case RenderPassType::Translucency: return "MaiX.TranslucencyPass";
    case RenderPassType::EditorPrimitive: return "MaiX.EditorPrimitivePass";
    case RenderPassType::Bloom: return "MaiX.BloomPass";
    case RenderPassType::PostProcess: return "MaiX.PostProcessPass";
    case RenderPassType::Present: return "MaiX.PresentPass";
    }
    return "MaiX.UnknownPass";
}
}

RenderPipeline::RenderPipeline()
    : m_TranslucencyPass(m_ForwardPass)
    , m_EditorPrimitivePass(m_ForwardPass)
    , m_ReflectionPass(m_ForwardPass, m_TranslucencyPass)
    , m_BloomPass(m_ForwardPass)
    , m_PostProcessPass(m_ForwardPass)
{
    // 顺序由 GPU 资源依赖决定：阴影和反射必须先于主颜色与 Present。
    m_Passes = {
        &m_ShadowPass,
        &m_ReflectionPass,
        &m_ForwardPass,
        &m_TranslucencyPass,
        &m_EditorPrimitivePass,
        &m_BloomPass,
        &m_PostProcessPass,
        &m_PresentPass
    };
}

bool RenderPipeline::Init()
{
    const bool forwardLoaded = m_ForwardPass.Init();
    const bool shadowLoaded = m_ShadowPass.Init(2048, 2048);
    const bool reflectionLoaded = m_ReflectionPass.Init();
    const bool editorPrimitivesLoaded = m_EditorPrimitivePass.Init();
    const bool bloomLoaded = m_BloomPass.Init();
    const bool postProcessLoaded = m_PostProcessPass.Init();
    const bool presentLoaded = m_PresentPass.Init();
    const bool profilerInitialized = m_GpuProfiler.Init();
    if (!profilerInitialized)
    {
        std::cerr
            << "[RenderPipeline] GPU timing disabled because Timer Query "
            << "initialization failed."
            << std::endl;
    }
    return forwardLoaded && shadowLoaded && reflectionLoaded &&
        editorPrimitivesLoaded && bloomLoaded && postProcessLoaded &&
        presentLoaded;
}

bool RenderPipeline::ReloadShaders()
{
    return m_ForwardPass.ReloadShaders() &&
        m_ShadowPass.ReloadShaders() &&
        m_ReflectionPass.ReloadShaders() &&
        m_EditorPrimitivePass.ReloadShaders() &&
        m_BloomPass.ReloadShaders() &&
        m_PostProcessPass.ReloadShaders() &&
        m_PresentPass.ReloadShaders();
}

void RenderPipeline::Execute(RenderPassContext& context)
{
    if (context.frame.viewportWidth == 0 ||
        context.frame.viewportHeight == 0)
        return;

    if (!EnsureRenderTargetExtents(
            context.frame.viewportWidth,
            context.frame.viewportHeight))
        return;

    RenderSubmissionStats& stats = RenderSubmissionStats::Get();
    stats.BeginFrame();
    m_GpuProfiler.BeginFrame();
    for (RenderPass* pass : m_Passes)
    {
        const RenderPassType type = pass->GetType();
        OpenGLStateCache::Get().Invalidate();
        stats.BeginPass(type);
        m_GpuProfiler.BeginPass(type);
        const GpuDebugScope debugScope(GetPassDebugName(type));
        pass->Execute(context);
        m_GpuProfiler.EndPass();
        stats.EndPass();
    }
    stats.EndFrame();
    m_GpuProfiler.EndFrame();
}

bool RenderPipeline::EnsureRenderTargetExtents(
    unsigned int viewportWidth,
    unsigned int viewportHeight)
{
    bool resized = false;
    const bool forwardMatches =
        m_ForwardPass.GetTargetWidth() == static_cast<int>(viewportWidth) &&
        m_ForwardPass.GetTargetHeight() == static_cast<int>(viewportHeight);
    if (!forwardMatches && !m_ForwardPass.Resize(viewportWidth, viewportHeight))
        return false;
    resized |= !forwardMatches;

    const RenderTargetExtent reflectionExtent =
        CalculateReflectionTargetExtent(viewportWidth, viewportHeight);
    const bool reflectionMatches =
        m_ReflectionPass.GetTargetWidth() ==
            static_cast<int>(reflectionExtent.width) &&
        m_ReflectionPass.GetTargetHeight() ==
            static_cast<int>(reflectionExtent.height);
    if (!reflectionMatches && !m_ReflectionPass.Resize(
            reflectionExtent.width, reflectionExtent.height))
        return false;
    resized |= !reflectionMatches;

    const RenderTargetExtent bloomExtent =
        CalculateBloomTargetExtent(viewportWidth, viewportHeight);
    const bool bloomMatches =
        m_BloomPass.GetTargetWidth() == static_cast<int>(bloomExtent.width) &&
        m_BloomPass.GetTargetHeight() == static_cast<int>(bloomExtent.height);
    if (!bloomMatches && !m_BloomPass.Resize(
            bloomExtent.width, bloomExtent.height))
        return false;
    resized |= !bloomMatches;

    const bool postProcessMatches =
        m_PostProcessPass.GetTargetWidth() == static_cast<int>(viewportWidth) &&
        m_PostProcessPass.GetTargetHeight() == static_cast<int>(viewportHeight);
    if (!postProcessMatches && !m_PostProcessPass.Resize(
            viewportWidth, viewportHeight))
        return false;
    resized |= !postProcessMatches;

    if (resized)
    {
        std::cout
            << "[RenderPipeline] Render targets resized: Forward="
            << viewportWidth << "x" << viewportHeight
            << ", Reflection=" << reflectionExtent.width << "x"
            << reflectionExtent.height
            << ", Bloom=" << bloomExtent.width << "x"
            << bloomExtent.height
            << ", PostProcess=" << viewportWidth << "x"
            << viewportHeight << std::endl;
    }

    return true;
}
