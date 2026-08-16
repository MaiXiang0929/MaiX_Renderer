// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/InstanceBuffer.h"
#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Shader.h"
#include "Renderer/View/LightRenderData.h"
#include "Renderer/View/InstanceTransformData.h"

struct RenderItem;

/// @brief 主颜色 Pass，负责场景正向渲染和主 HDR/颜色目标。
class ForwardPass final : public RenderPass
{
public:
    ~ForwardPass() override;

    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override { return RenderPassType::Forward; }
    void Execute(RenderPassContext& context) override;

    /// ReflectionPass 复用同一套材质 Shader，确保反射与主画面一致。
    void RenderSurface(RenderPassContext& context, RenderView& view);
    void RenderTranslucentSurface(
        RenderPassContext& context,
        RenderView& view);
    void RenderSkybox(
        RenderPassContext& context,
        const cy::Matrix4f& view);

    GLuint GetColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    GLuint GetDepthTexture() const { return m_Framebuffer.GetDepthTexture(); }
    int GetTargetWidth() const { return m_Framebuffer.GetWidth(); }
    int GetTargetHeight() const { return m_Framebuffer.GetHeight(); }
    void BindColorTarget() const { m_Framebuffer.Bind(); }
    void UnbindColorTarget(bool generateMipmaps = true) const
    {
        m_Framebuffer.Unbind();
        if (generateMipmaps)
            m_Framebuffer.GenerateMipmaps();
    }

private:
    static constexpr GLuint ForwardLightsBindingPoint = 0;

    bool BindForwardLightsBlock(const Shader& shader) const;
    LightUploadData UploadLights(
        const RenderView& view,
        LightId shadowLightId);
    void RenderItems(
        RenderPassContext& context,
        RenderView& renderView,
        const std::vector<RenderItem>& items,
        bool allowWireframeOverlay);
    void RenderOpaqueBatches(
        RenderPassContext& context,
        RenderView& renderView);
    void PrepareSurfaceShader(
        RenderPassContext& context,
        RenderView& renderView,
        Shader& shader,
        bool tessellationEnabled);

    Shader m_StandardShader;
    Shader m_InstancedStandardShader;
    Shader m_TessellationShader;
    Shader m_SkyboxShader;
    Shader m_GroundShader;
    Framebuffer m_Framebuffer;
    InstanceBuffer m_InstanceBuffer;
    GLuint m_LightBuffer = 0;
    bool m_LightLimitWarningIssued = false;
    std::vector<InstanceTransformData> m_InstanceTransforms;
};
