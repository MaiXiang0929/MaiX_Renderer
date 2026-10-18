// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/Rendering/RHI/RHIDevice.h"

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"

class EditorPrimitivePass final : public RenderPass
{
public:
    explicit EditorPrimitivePass(RHIDevice& device) : m_Device(device), m_OverlayTarget(device), m_BillboardShader(device), m_LineShader(device) {}
    ~EditorPrimitivePass() override;

    EditorPrimitivePass(const EditorPrimitivePass&) = delete;
    EditorPrimitivePass& operator=(const EditorPrimitivePass&) = delete;

    bool Init();
    bool Resize(unsigned int width, unsigned int height);
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::EditorPrimitive;
    }
    void Execute(RenderPassContext& context) override;

    RHITextureHandle GetColorTexture() const
    {
        return m_OverlayTarget.GetColorTexture();
    }
    int GetTargetWidth() const { return m_OverlayTarget.GetWidth(); }
    const Framebuffer& GetTarget() const { return m_OverlayTarget; }
    int GetTargetHeight() const { return m_OverlayTarget.GetHeight(); }

private:
    RHIDevice& m_Device;
    void CreateBillboardMesh();
    void CreateSpotConeMesh();
    void DrawBillboard(
        const RenderPassContext& context,
        const cy::Vec3f& position,
        const cy::Vec3f& color);
    void DrawSpotCone(
        const RenderPassContext& context,
        const struct LightSceneProxy& light);

    Framebuffer m_OverlayTarget;
    Shader m_BillboardShader;
    Shader m_LineShader;
    RHIVertexInputHandle m_BillboardVao = 0;
    RHIBufferHandle m_BillboardVbo = 0;
    RHIVertexInputHandle m_ConeVao = 0;
    RHIBufferHandle m_ConeVbo = 0;
    unsigned m_ConeVertexCount = 0;
};
