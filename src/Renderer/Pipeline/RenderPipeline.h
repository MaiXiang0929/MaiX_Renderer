// SPDX-License-Identifier: MIT
/// @file RenderPipeline.h
/// @brief 渲染管线类的头文件
/// @details 固定顺序执行现有 Pass，并验证它们的当帧资源访问契约。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <vector>

#include "Renderer/Passes/ForwardPass.h"
#include "Renderer/Passes/OutlinePass.h"
#include "Renderer/Passes/SSAOPass.h"
#include "Renderer/Passes/EditorPrimitivePass.h"
#include "Renderer/Passes/BloomPass.h"
#include "Renderer/Passes/PostProcessPass.h"
#include "Renderer/Passes/PresentPass.h"
#include "Renderer/Passes/ReflectionPass.h"
#include "RenderPass.h"
#include "Renderer/Passes/ShadowPass.h"
#include "Renderer/Passes/TranslucencyPass.h"
#include "Renderer/Diagnostics/GpuPassProfiler.h"

/// @brief Owns and executes the renderer's ordered pass sequence.
class RenderPipeline
{
public:
    RenderPipeline();

    bool Init();
    bool ReloadShaders();
    void Execute(RenderPassContext& context);
    GLuint GetFinalColorTexture() const { return m_FinalColorTexture; }

    const std::vector<RenderPass*>& GetPasses() const
    {
        return m_Passes;
    }
    const GpuTimingSnapshot& GetGpuTimingSnapshot() const
    {
        return m_GpuProfiler.GetSnapshot();
    }

private:
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
    GLuint m_FinalColorTexture = 0;
    std::string m_LastResourceError;
};
