// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/EditorPrimitivePass.h"

#include <cmath>
#include <vector>


#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"
#include "Engine/Rendering/Renderer/View/RenderView.h"

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr int ConeSegments = 32;
constexpr int ConeSideCount = 8;
}

EditorPrimitivePass::~EditorPrimitivePass()
{
    m_Device.DestroyVertexInput(m_ConeVao); m_Device.DestroyBuffer(m_ConeVbo);
    m_Device.DestroyVertexInput(m_BillboardVao); m_Device.DestroyBuffer(m_BillboardVbo);
}

bool EditorPrimitivePass::Init()
{
    CreateBillboardMesh();
    CreateSpotConeMesh();
    return ReloadShaders();
}

bool EditorPrimitivePass::Resize(unsigned int width, unsigned int height)
{
    FramebufferSpecification specification;
    specification.width = static_cast<int>(width);
    specification.height = static_cast<int>(height);
    specification.colorFormat = FramebufferColorFormat::RGBA8;
    specification.depthStencilEnabled = false;
    specification.mipmapsEnabled = true;
    return m_OverlayTarget.Init(specification);
}

bool EditorPrimitivePass::ReloadShaders()
{
    const bool billboardLoaded = m_BillboardShader.Load(
        "Shaders/debug/billboard.vert",
        "Shaders/debug/billboard.frag");
    const bool lineLoaded = m_LineShader.Load(
        "Shaders/debug/light_cone.vert",
        "Shaders/debug/light_cone.frag");
    return billboardLoaded && lineLoaded;
}

void EditorPrimitivePass::Execute(RenderPassContext& context)
{
    context.Resources().BeginTarget(PassResourceId::Overlay);

    if (!context.frame.editorPrimitivesEnabled ||
        context.mainView.lights.empty())
    {
        context.Resources().EndTarget(PassResourceId::Overlay);
        context.Resources().Publish(PassResourceId::Overlay);
        return;
    }

    RHIScopedPipelineState stateScope(m_Device.Commands());
    auto state=stateScope.saved; state.depthTest=false; state.depthWrite=false; state.blend=true;
    state.sourceRgb=state.sourceAlpha=RHIBlendFactor::One;
    state.destinationRgb=state.destinationAlpha=RHIBlendFactor::OneMinusSrcAlpha;
    m_Device.Commands().SetPipelineState(state);

    for (const LightSceneProxy& light : context.mainView.lights)
    {
        if (light.type == LightType::Directional)
            continue;

        DrawBillboard(context, light.position, light.color);
        if (light.type == LightType::Spot)
            DrawSpotCone(context, light);
    }

    context.Resources().EndTarget(PassResourceId::Overlay);
    context.Resources().Publish(PassResourceId::Overlay);
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

    m_BillboardVbo=m_Device.CreateBuffer({RHIBufferUsage::Vertex,sizeof(vertices)},vertices);
    if(m_BillboardVbo==0) throw RHIError("Billboard buffer allocation failed");
    m_BillboardVao=m_Device.CreateVertexInput({m_BillboardVbo,{},4*sizeof(float),{{0,2,0},{1,2,2*sizeof(float)}}});
    if(m_BillboardVao==0) throw RHIError("Billboard vertex input failed");
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

    m_ConeVertexCount=static_cast<unsigned>(vertices.size()/3);
    m_ConeVbo=m_Device.CreateBuffer({RHIBufferUsage::Vertex,vertices.size()*sizeof(float)},vertices.data());
    if(m_ConeVbo==0) throw RHIError("Cone buffer allocation failed");
    m_ConeVao=m_Device.CreateVertexInput({m_ConeVbo,{},3*sizeof(float),{{0,3,0}}});
    if(m_ConeVao==0) throw RHIError("Cone vertex input failed");
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
        "viewportWidth", static_cast<float>(GetTargetWidth()));
    m_BillboardShader.SetFloat(
        "viewportHeight", static_cast<float>(GetTargetHeight()));
    m_BillboardShader.SetFloat("iconSizePixels", 28.0f);
    m_Device.Commands().Draw(m_BillboardVao,RHITopology::Triangles,6,false);
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
    m_Device.Commands().Draw(m_ConeVao,RHITopology::Lines,m_ConeVertexCount,false);
}
