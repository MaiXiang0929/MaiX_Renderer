// SPDX-License-Identifier: MIT
/// @file RenderPipeline.h
/// @brief 渲染管线类的头文件
/// @details 固定顺序执行现有 Pass，并验证它们的当帧资源访问契约。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <vector>

#include "Engine/Rendering/Passes/ForwardPass.h"
#include "Engine/Rendering/Passes/OutlinePass.h"
#include "Engine/Rendering/Passes/SSAOPass.h"
#include "Engine/Rendering/Passes/EditorPrimitivePass.h"
#include "Engine/Rendering/Passes/BloomPass.h"
#include "Engine/Rendering/Passes/PostProcessPass.h"
#include "Engine/Rendering/Passes/PresentPass.h"
#include "Engine/Rendering/Passes/ReflectionPass.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Passes/ShadowPass.h"
#include "Engine/Rendering/Passes/TranslucencyPass.h"
#include "Engine/Rendering/Renderer/Diagnostics/GpuPassProfiler.h"

/// @brief Owns and executes the renderer's ordered pass sequence.
class RenderPipeline
{
public:
    explicit RenderPipeline(RHIDevice& device);

    bool Init();
    bool ReloadShaders();
    void Execute(RenderPassContext& context);
    RHITextureHandle GetFinalColorTexture() const { return m_FinalColorTexture; }

    const std::vector<RenderPass*>& GetPasses() const
    {
        return m_Passes;
    }
    const GpuTimingSnapshot& GetGpuTimingSnapshot() const
    {
        return m_GpuProfiler.GetSnapshot();
    }

private:
    RHIDevice& m_Device;
    void BindFrameResources(FrameResources& resources, const RenderPassContext& context) const;
    bool EnsureRenderTargetExtents(
        unsigned int viewportWidth,
        unsigned int viewportHeight);

    ForwardPass m_ForwardPass;
    OutlinePass m_OutlinePass;
    TranslucencyPass m_TranslucencyPass;
    SSAOPass m_SSAOPass;
    EditorPrimitivePass m_EditorPrimitivePass;
    ShadowPass m_ShadowPass;
    ReflectionPass m_ReflectionPass;
    BloomPass m_BloomPass;
    PostProcessPass m_PostProcessPass;
    PresentPass m_PresentPass;
    GpuPassProfiler m_GpuProfiler;
    std::vector<RenderPass*> m_Passes;
    RHITextureHandle m_FinalColorTexture = 0;
    std::string m_LastResourceError;
};
