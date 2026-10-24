// SPDX-License-Identifier: MIT
/// @file ShadowPass.h
/// @brief 阴影渲染阶段的头文件
/// @details 该文件声明了 ShadowPass 类，它负责生成阴影贴图并提供深度纹理给后续 Pass 使用。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <vector>

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"
#include "Engine/Rendering/Renderer/Resources/InstanceBuffer.h"
#include "Engine/Rendering/Renderer/View/InstanceTransformData.h"

/// @brief 逐灯/级联/六面绘制；所有阴影数组均由此 Pass 拥有。
class ShadowPass final : public RenderPass
{
public:
    explicit ShadowPass(RHIDevice& device) : m_Device(device), m_StandardShader(device), m_InstancedStandardShader(device), m_TessellationShader(device), m_PointShader(device), m_PointInstancedShader(device), m_PointTessellationShader(device), m_InstanceBuffer(device) {}
    ~ShadowPass() override;
    bool Init(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override { return RenderPassType::Shadow; }
    void Execute(RenderPassContext& context) override;

    bool PrepareTargets(const ShadowFrame& frame);
    const RHITarget& GetTarget() const { return m_ArrayTarget; }
    const RHITarget& GetCubeTarget() const { return m_CubeTarget; }
    unsigned Resolution() const { return m_Resolution; }
    unsigned PointResolution() const { return m_PointResolution; }
    unsigned Layers() const { return m_Layers; }
    unsigned CubeLayers() const { return m_CubeLayers; }

private:
    RHIDevice& m_Device;
    Shader m_StandardShader;
    Shader m_InstancedStandardShader;
    Shader m_TessellationShader;
    Shader m_PointShader;
    Shader m_PointInstancedShader;
    Shader m_PointTessellationShader;
    RHITarget m_ArrayTarget, m_CubeTarget;
    unsigned m_Resolution = 0, m_PointResolution = 0, m_Layers = 0, m_CubeLayers = 0;
    void RenderDepth(RenderPassContext& context, const RenderView& view, const ShadowViewFrame& shadowView);
    InstanceBuffer m_InstanceBuffer;
    std::vector<InstanceTransformData> m_InstanceTransforms;
    std::vector<std::size_t> m_BatchInstanceCounts;
};
