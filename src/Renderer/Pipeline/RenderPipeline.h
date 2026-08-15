// SPDX-License-Identifier: MIT
/// @file RenderPipeline.h
/// @brief 渲染管线类的头文件
/// @details 该文件声明了 RenderPipeline 类，它直接拥有并按固定顺序执行四个真实渲染 Pass：ForwardPass、ShadowPass、ReflectionPass 和 PresentPass。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <vector>

#include "Renderer/Passes/ForwardPass.h"
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

    const std::vector<RenderPass*>& GetPasses() const
    {
        return m_Passes;
    }
    const GpuTimingSnapshot& GetGpuTimingSnapshot() const
    {
        return m_GpuProfiler.GetSnapshot();
    }

private:
    bool EnsureRenderTargetExtents(
        unsigned int viewportWidth,
        unsigned int viewportHeight);

    ForwardPass m_ForwardPass;
    TranslucencyPass m_TranslucencyPass;
    EditorPrimitivePass m_EditorPrimitivePass;
    ShadowPass m_ShadowPass;
    ReflectionPass m_ReflectionPass;
    BloomPass m_BloomPass;
    PostProcessPass m_PostProcessPass;
    PresentPass m_PresentPass;
    GpuPassProfiler m_GpuProfiler;
    std::vector<RenderPass*> m_Passes;
};
