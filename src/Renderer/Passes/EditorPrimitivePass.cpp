// SPDX-License-Identifier: MIT
#include "EditorPrimitivePass.h"

#include <cmath>
#include <vector>

#include "Renderer/Passes/ForwardPass.h"
#include "Renderer/Core/OpenGLStateCache.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Scene/LightSceneProxy.h"
#include "Renderer/View/RenderView.h"

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr int ConeSegments = 32;
constexpr int ConeSideCount = 8;
}

EditorPrimitivePass::~EditorPrimitivePass()
{
    if (m_ConeVbo != 0) glDeleteBuffers(1, &m_ConeVbo);
    if (m_ConeVao != 0) glDeleteVertexArrays(1, &m_ConeVao);
    if (m_BillboardVbo != 0) glDeleteBuffers(1, &m_BillboardVbo);
    if (m_BillboardVao != 0) glDeleteVertexArrays(1, &m_BillboardVao);
}

bool EditorPrimitivePass::Init()
{
    CreateBillboardMesh();
    CreateSpotConeMesh();
    return ReloadShaders();
}

bool EditorPrimitivePass::ReloadShaders()
{
    const bool billboardLoaded = m_BillboardShader.Load(
        "assets/shaders/debug/billboard.vert",
        "assets/shaders/debug/billboard.frag");
    const bool lineLoaded = m_LineShader.Load(
        "assets/shaders/debug/light_cone.vert",
        "assets/shaders/debug/light_cone.frag");
    return billboardLoaded && lineLoaded;
}

void EditorPrimitivePass::Execute(RenderPassContext& context)
{
    if (!context.frame.editorPrimitivesEnabled ||
        context.mainView.lights.empty())
        return;

    m_ForwardPass.BindColorTarget();

    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthWriteEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);
    GLint blendSourceRgb = GL_ONE;
    GLint blendDestinationRgb = GL_ZERO;
    GLint blendSourceAlpha = GL_ONE;
    GLint blendDestinationAlpha = GL_ZERO;
    GLint blendEquationRgb = GL_FUNC_ADD;
    GLint blendEquationAlpha = GL_FUNC_ADD;
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const LightSceneProxy& light : context.mainView.lights)
    {
        if (light.type == LightType::Directional)
            continue;

        DrawBillboard(context, light.position, light.color);
        if (light.type == LightType::Spot)
            DrawSpotCone(context, light);
    }

    if (!blendEnabled) glDisable(GL_BLEND);
    glBlendFuncSeparate(
        blendSourceRgb, blendDestinationRgb,
        blendSourceAlpha, blendDestinationAlpha);
    glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    glDepthMask(depthWriteEnabled);

    m_ForwardPass.UnbindColorTarget();
}

void EditorPrimitivePass::CreateBillboardMesh()
{
    const float vertices[] = {
        -0.5f,  0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_BillboardVao);
    glGenBuffers(1, &m_BillboardVbo);
    glBindVertexArray(m_BillboardVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_BillboardVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void EditorPrimitivePass::CreateSpotConeMesh()
{
    std::vector<float> vertices;
    vertices.reserve((ConeSegments * 2 + ConeSideCount * 2) * 3);
    for (int segment = 0; segment < ConeSegments; ++segment)
    {
        const float angle0 = 2.0f * Pi * segment / ConeSegments;
        const float angle1 = 2.0f * Pi * (segment + 1) / ConeSegments;
        vertices.insert(vertices.end(), {
            std::cos(angle0), std::sin(angle0), 1.0f,
            std::cos(angle1), std::sin(angle1), 1.0f });
    }
    for (int side = 0; side < ConeSideCount; ++side)
    {
        const float angle = 2.0f * Pi * side / ConeSideCount;
        vertices.insert(vertices.end(), {
            0.0f, 0.0f, 0.0f,
            std::cos(angle), std::sin(angle), 1.0f });
    }

    m_ConeVertexCount = static_cast<GLsizei>(vertices.size() / 3);
    glGenVertexArrays(1, &m_ConeVao);
    glGenBuffers(1, &m_ConeVbo);
    glBindVertexArray(m_ConeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_ConeVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void EditorPrimitivePass::DrawBillboard(
    const RenderPassContext& context,
    const cy::Vec3f& position,
    const cy::Vec3f& color)
{
    m_BillboardShader.Bind();
    m_BillboardShader.SetMatrix4(
        "viewProjection", &context.mainView.viewProjection.cell[0]);
    m_BillboardShader.SetVec3("worldPosition", position.x, position.y, position.z);
    m_BillboardShader.SetVec3("iconColor", color.x, color.y, color.z);
    m_BillboardShader.SetFloat(
        "viewportWidth", static_cast<float>(m_ForwardPass.GetTargetWidth()));
    m_BillboardShader.SetFloat(
        "viewportHeight", static_cast<float>(m_ForwardPass.GetTargetHeight()));
    m_BillboardShader.SetFloat("iconSizePixels", 28.0f);
    RenderSubmissionStats::Get().RecordMeshDraw(m_BillboardVao);
    OpenGLStateCache::Get().BindVertexArray(m_BillboardVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void EditorPrimitivePass::DrawSpotCone(
    const RenderPassContext& context,
    const LightSceneProxy& light)
{
    m_LineShader.Bind();
    m_LineShader.SetMatrix4(
        "viewProjection", &context.mainView.viewProjection.cell[0]);
    m_LineShader.SetVec3(
        "lightPosition", light.position.x, light.position.y, light.position.z);
    m_LineShader.SetVec3(
        "lightDirection", light.direction.x, light.direction.y, light.direction.z);
    m_LineShader.SetVec3(
        "lineColor", light.color.x, light.color.y, light.color.z);
    m_LineShader.SetFloat("lightRange", light.range);
    m_LineShader.SetFloat("outerConeAngle", light.outerConeAngle);
    RenderSubmissionStats::Get().RecordMeshDraw(m_ConeVao);
    OpenGLStateCache::Get().BindVertexArray(m_ConeVao);
    glDrawArrays(GL_LINES, 0, m_ConeVertexCount);
}
