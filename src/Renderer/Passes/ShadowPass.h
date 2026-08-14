// SPDX-License-Identifier: MIT
/// @file ShadowPass.h
/// @brief 阴影渲染阶段的头文件
/// @details 该文件声明了 ShadowPass 类，它负责生成阴影贴图并提供深度纹理给后续 Pass 使用。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <vector>

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Shader.h"
#include "Renderer/Resources/InstanceBuffer.h"
#include "Renderer/Resources/ShadowMap.h"
#include "Renderer/View/InstanceTransformData.h"

/// @brief 阴影 Pass：普通和 Tessellation 路径共享同一张主深度纹理。
class ShadowPass final : public RenderPass
{
public:
    bool Init(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override { return RenderPassType::Shadow; }
    void Execute(RenderPassContext& context) override;

    GLuint GetDepthTexture() const { return m_ShadowMap.GetDepthTexture(); }

private:
    Shader m_StandardShader;
    Shader m_InstancedStandardShader;
    Shader m_TessellationShader;
    ShadowMap m_ShadowMap;
    InstanceBuffer m_InstanceBuffer;
    std::vector<InstanceTransformData> m_InstanceTransforms;
    std::vector<std::size_t> m_BatchInstanceCounts;
};
