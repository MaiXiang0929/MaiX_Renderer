// SPDX-License-Identifier: MIT
/// @file RenderPipeline.cpp
/// @brief 渲染管线类的实现文件
/// @details 该文件实现了 RenderPipeline 类的核心功能，包括初始化各个渲染阶段、执行渲染流程等。
/// @author MaiX
/// @date 2026-08-11


#include "Engine/Rendering/Renderer/Pipeline/RenderPipeline.h"

#include <iostream>
#include <array>

#include "Engine/Rendering/Renderer/Diagnostics/GpuDebugScope.h"
#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"

#include "Engine/Rendering/Renderer/Pipeline/RenderTargetSizing.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Rendering/Renderer/Resources/CubemapTexture.h"

namespace
{
RenderResourceBinding Snapshot(const Framebuffer& target, bool depth = false)
{
    const auto format = depth ? ResourceFormat::Depth24Stencil8
        : target.GetColorFormat() == FramebufferColorFormat::RGBA16F ? ResourceFormat::RGBA16F
        : target.GetColorFormat() == FramebufferColorFormat::R8 ? ResourceFormat::R8 : ResourceFormat::RGBA8;
    return {depth ? target.GetDepthTexture() : target.GetColorTexture(),
        target.GetFramebufferId(), target.GetDepthTexture(),
        static_cast<unsigned int>(target.GetWidth()), static_cast<unsigned int>(target.GetHeight()),
        format, target.HasDepthStencil(), false, target.HasMipmaps(), target.GetSecondaryColorTexture()};
}
const char* GetPassDebugName(RenderPassType type)
{
    switch (type)
    {
    case RenderPassType::Shadow: return "MaiX.ShadowPass";
    case RenderPassType::ShadowPreview: return "MaiX.ShadowPreviewPass";
    case RenderPassType::Reflection: return "MaiX.ReflectionPass";
    case RenderPassType::Forward: return "MaiX.ForwardPass";
    case RenderPassType::Outline: return "MaiX.OutlinePass";
    case RenderPassType::Translucency: return "MaiX.TranslucencyPass";
    case RenderPassType::SSAO: return "MaiX.SSAOPass";
    case RenderPassType::EditorPrimitive: return "MaiX.EditorPrimitivePass";
    case RenderPassType::Bloom: return "MaiX.BloomPass";
    case RenderPassType::PostProcess: return "MaiX.PostProcessPass";
    case RenderPassType::Present: return "MaiX.PresentPass";
    case RenderPassType::Count: break;
    }
    return "MaiX.UnknownPass";
}
}

RenderPipeline::RenderPipeline(RHIDevice& device)
    : m_Device(device), m_ForwardPass(device), m_OutlinePass(device), m_TranslucencyPass(m_ForwardPass),
      m_SSAOPass(device), m_EditorPrimitivePass(device), m_ShadowPass(device), m_ShadowPreviewPass(device),
      m_ReflectionPass(device, m_ForwardPass, m_TranslucencyPass), m_BloomPass(device),
      m_PostProcessPass(device), m_PresentPass(device), m_GpuProfiler(device)
{
    // 顺序由 GPU 资源依赖决定：阴影和反射必须先于主颜色与 Present。
    m_Passes = {
        &m_ShadowPass,
        &m_ShadowPreviewPass,
        &m_ReflectionPass,
        &m_ForwardPass,
        &m_OutlinePass,
        &m_TranslucencyPass,
        &m_SSAOPass,
        &m_BloomPass,
        &m_PostProcessPass,
        &m_EditorPrimitivePass,
        &m_PresentPass
    };
}

bool RenderPipeline::Init()
{
    // 所有当前开关组合都验证固定顺序；不执行拓扑排序，也不增加 GPU 工作。
    for (unsigned int flags = 0; flags < 16; ++flags)
    {
        std::array<PassResourceContract, static_cast<std::size_t>(RenderPassType::Count)> contracts;
        for (std::size_t i = 0; i < m_Passes.size(); ++i)
            contracts[i] = BuildPassResourceContract(m_Passes[i]->GetType(),
                {(flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0, (flags & 8) != 0});
        std::string error;
        if (!ValidatePassResourceSequence(contracts.data(), contracts.size(), error))
        {
            std::cerr << "[PassResources] " << error << std::endl;
            return false;
        }
    }
    const bool forwardLoaded = m_ForwardPass.Init();
    const bool outlineLoaded = m_OutlinePass.Init();
    const bool ssaoLoaded = m_SSAOPass.Init();
    const bool shadowLoaded = m_ShadowPass.Init(2048, 2048);
    const bool previewLoaded = m_ShadowPreviewPass.Init();
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
    return forwardLoaded && outlineLoaded && ssaoLoaded && shadowLoaded && previewLoaded && reflectionLoaded &&
        editorPrimitivesLoaded && bloomLoaded && postProcessLoaded &&
        presentLoaded;
}

bool RenderPipeline::ReloadShaders()
{
    return m_ForwardPass.ReloadShaders() &&
        m_OutlinePass.ReloadShaders() &&
        m_SSAOPass.ReloadShaders() &&
        m_ShadowPass.ReloadShaders() &&
        m_ShadowPreviewPass.ReloadShaders() &&
        m_ReflectionPass.ReloadShaders() &&
        m_EditorPrimitivePass.ReloadShaders() &&
        m_BloomPass.ReloadShaders() &&
        m_PostProcessPass.ReloadShaders() &&
        m_PresentPass.ReloadShaders();
}

void RenderPipeline::Execute(RenderPassContext& context)
{
    m_FinalColorTexture = 0;
    m_ShadowPreviewTexture = 0;
    context.passResources = nullptr;
    if (context.frame.viewportWidth == 0 ||
        context.frame.viewportHeight == 0)
        return;

    if (!EnsureRenderTargetExtents(
            context.frame.viewportWidth,
            context.frame.viewportHeight))
        return;

    if (context.frame.shadowsEnabled && !m_ShadowPass.PrepareTargets(context.frame.shadowFrame))
        return;
    const bool preview = context.frame.shadowsEnabled && context.frame.shadowPreviewLight >= 0 &&
        context.frame.shadowPreviewLight < static_cast<int>(context.frame.shadowFrame.lights.size());
    if (preview && !m_ShadowPreviewPass.Prepare()) return;

    FrameResources resources;
    resources.BeginFrame(context.frame.viewportWidth, context.frame.viewportHeight);
    BindFrameResources(resources, context);
    const PassResourceFeatures features{context.frame.shadowsEnabled,
        context.postProcess.ssaoEnabled, context.postProcess.bloomEnabled, preview};
    RenderSubmissionStats& stats = RenderSubmissionStats::Get();
    stats.BeginFrame();
    m_GpuProfiler.BeginFrame();
    for (RenderPass* pass : m_Passes)
    {
        const RenderPassType type = pass->GetType();
        const auto contract = BuildPassResourceContract(type, features);
        PassResources access(resources, contract, &m_Device.Commands());
        bool passStarted = false;
        try
        {
            // 先检查所有输入与输出，缺失资源时不能提交该 Pass 的 GPU 命令。
            access.Validate();
            context.passResources = &access;
            m_Device.Commands().InvalidateState();
            stats.BeginPass(type);
            m_GpuProfiler.BeginPass(type);
            passStarted = true;
            const GpuDebugScope debugScope(m_Device.Commands(), GetPassDebugName(type));
            m_Device.Commands().SetPipelineState(RHIPipelineState{});
            pass->Execute(context);
            access.Complete();
        }
        catch (const RHIError& error)
        {
            if (m_LastResourceError != error.what())
                std::cerr << "[PassResources] " << error.what() << std::endl;
            m_LastResourceError = error.what();
            context.passResources = nullptr;
            if (passStarted)
            {
                m_GpuProfiler.EndPass();
                stats.EndPass();
            }
            m_Device.Commands().EndTarget();
            m_Device.Commands().InvalidateState();
            break;
        }
        context.passResources = nullptr;
        m_GpuProfiler.EndPass();
        stats.EndPass();
    }
    stats.EndFrame();
    m_GpuProfiler.EndFrame();
    // 已分配不等于本帧有效；失败或 Resize 中断后不向 Viewport 暴露旧内容。
    m_FinalColorTexture = resources.GetFinalTexture();
    if (m_FinalColorTexture && resources.IsProduced(PassResourceId::ShadowPreview))
        m_ShadowPreviewTexture = m_ShadowPreviewPass.GetTarget().GetColorTexture();
    if (m_FinalColorTexture != 0) m_LastResourceError.clear();
}

void RenderPipeline::BindFrameResources(FrameResources& resources, const RenderPassContext& context) const
{
    using Id = PassResourceId;
    resources.Bind(Id::Environment, {context.cubemap.GetID(), 0, 0, context.cubemap.Size(), context.cubemap.Size(),
        context.cubemap.IsHdr() ? ResourceFormat::Cubemap16F : ResourceFormat::Cubemap, false, true});
    const auto& lighting = context.cubemap.Lighting();
    resources.Bind(Id::Irradiance, {lighting.Irradiance(),0,0,
        EnvironmentLighting::IrradianceSize,EnvironmentLighting::IrradianceSize,
        ResourceFormat::Cubemap16F,false,true});
    resources.Bind(Id::PrefilteredEnvironment, {lighting.Prefiltered(),0,0,
        EnvironmentLighting::PrefilterSize,EnvironmentLighting::PrefilterSize,
        ResourceFormat::Cubemap16F,false,true,true});
    resources.Bind(Id::BrdfLut, {lighting.Brdf(),0,0,
        EnvironmentLighting::BrdfSize,EnvironmentLighting::BrdfSize,
        ResourceFormat::RGBA16F,false,true});
    const auto& shadow = m_ShadowPass.GetTarget();
    RenderResourceBinding shadowBinding{shadow.depth, shadow.handle, shadow.depth,
        m_ShadowPass.Resolution(), m_ShadowPass.Resolution(), ResourceFormat::DepthArray, true, false};
    shadowBinding.layers = m_ShadowPass.Layers();
    shadowBinding.requiredLayers = std::max(context.frame.shadowFrame.layers2D, 1u);
    resources.Bind(Id::ShadowDepth, shadowBinding);
    const auto& cube = m_ShadowPass.GetCubeTarget();
    RenderResourceBinding cubeBinding{cube.depth, cube.handle, cube.depth,
        m_ShadowPass.PointResolution(), m_ShadowPass.PointResolution(), ResourceFormat::DepthCubeArray, true, false};
    cubeBinding.layers = m_ShadowPass.CubeLayers();
    cubeBinding.requiredLayers = std::max(context.frame.shadowFrame.cubeCount, 1u) * 6;
    resources.Bind(Id::ShadowCubeDepth, cubeBinding);
    resources.Bind(Id::ShadowPreview, Snapshot(m_ShadowPreviewPass.GetTarget()));
    resources.Bind(Id::ReflectionColor, Snapshot(m_ReflectionPass.GetTarget()));
    const auto mainColor = Snapshot(m_ForwardPass.GetTarget());
    auto indirect = mainColor;
    indirect.texture = m_ForwardPass.GetTarget().GetSecondaryColorTexture();
    indirect.otherColorTexture = mainColor.texture;
    indirect.mipmaps = false;
    resources.Bind(Id::ForwardIndirect, indirect);
    resources.Bind(Id::OutlinedIndirect, indirect);
    resources.Bind(Id::SceneIndirect, indirect);
    resources.Bind(Id::ForwardColor, mainColor);
    resources.Bind(Id::OutlinedColor, mainColor);
    resources.Bind(Id::SceneHdrColor, mainColor);
    resources.Bind(Id::ForwardDepth, Snapshot(m_ForwardPass.GetTarget(), true));
    resources.Bind(Id::SsaoAO, Snapshot(m_SSAOPass.GetAoTarget()));
    resources.Bind(Id::SsaoColor, Snapshot(m_SSAOPass.GetTarget()));
    resources.Bind(Id::Bloom, Snapshot(m_BloomPass.GetTarget()));
    resources.Bind(Id::PostColor, Snapshot(m_PostProcessPass.GetTarget()));
    resources.Bind(Id::Overlay, Snapshot(m_EditorPrimitivePass.GetTarget()));
    resources.Bind(Id::FinalColor, Snapshot(m_PresentPass.GetTarget()));
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
    const bool bloomMatches = m_BloomPass.TargetsMatch(
        static_cast<int>(bloomExtent.width),
        static_cast<int>(bloomExtent.height));
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

    const bool presentMatches =
        m_PresentPass.GetTargetWidth() == static_cast<int>(viewportWidth) &&
        m_PresentPass.GetTargetHeight() == static_cast<int>(viewportHeight);
    if (!presentMatches && !m_PresentPass.Resize(viewportWidth, viewportHeight))
        return false;
    resized |= !presentMatches;

    const RenderTargetExtent ssaoExtent =
        CalculateSsaoTargetExtent(viewportWidth, viewportHeight);
    const bool ssaoMatches = m_SSAOPass.TargetsMatch(
        static_cast<int>(ssaoExtent.width),
        static_cast<int>(ssaoExtent.height),
        static_cast<int>(viewportWidth),
        static_cast<int>(viewportHeight));
    if (!ssaoMatches && !m_SSAOPass.Resize(ssaoExtent.width, ssaoExtent.height,
            viewportWidth, viewportHeight))
        return false;
    resized |= !ssaoMatches;

    const RenderTargetExtent editorOverlayExtent =
        CalculateEditorOverlayTargetExtent(viewportWidth, viewportHeight);
    const bool editorOverlayMatches =
        m_EditorPrimitivePass.GetTargetWidth() ==
            static_cast<int>(editorOverlayExtent.width) &&
        m_EditorPrimitivePass.GetTargetHeight() ==
            static_cast<int>(editorOverlayExtent.height);
    if (!editorOverlayMatches && !m_EditorPrimitivePass.Resize(
            editorOverlayExtent.width, editorOverlayExtent.height))
        return false;
    resized |= !editorOverlayMatches;

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
            << viewportHeight
            << ", SSAO=" << ssaoExtent.width << "x"
            << ssaoExtent.height
            << ", EditorOverlay=" << editorOverlayExtent.width << "x"
            << editorOverlayExtent.height << std::endl;
    }

    return true;
}
