// SPDX-License-Identifier: MIT
#pragma once

#include <glad/glad.h>

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Resources/Shader.h"

class EditorPrimitivePass final : public RenderPass
{
public:
    EditorPrimitivePass() = default;
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

    GLuint GetColorTexture() const
    {
        return m_OverlayTarget.GetColorTexture();
    }
    int GetTargetWidth() const { return m_OverlayTarget.GetWidth(); }
    int GetTargetHeight() const { return m_OverlayTarget.GetHeight(); }

private:
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
    GLuint m_BillboardVao = 0;
    GLuint m_BillboardVbo = 0;
    GLuint m_ConeVao = 0;
    GLuint m_ConeVbo = 0;
    GLsizei m_ConeVertexCount = 0;
};
