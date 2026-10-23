// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/OutlinePass.h"

#include <algorithm>

#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Rendering/Material/Material.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include "Engine/Rendering/Renderer/View/RenderView.h"

namespace
{
bool ShouldDrawOutline(const RenderItem& item)
{
    if (item.mesh == nullptr || item.material == nullptr)
        return false;

    const MaterialProperties& properties = item.material->GetProperties();
    return properties.shadingModel == ShadingModel::Toon &&
        properties.outlineEnabled &&
        properties.outlineThickness > MinimumOutlineThickness;
}
}

bool OutlinePass::Init()
{
    return ReloadShaders();
}

bool OutlinePass::ReloadShaders()
{
    return m_Shader.Load(
        "Shaders/npr/outline.vert",
        "Shaders/npr/outline.frag");
}

void OutlinePass::Execute(RenderPassContext& context)
{
    // The base mesh cannot match tessellation displacement, so skip this
    // deliberately unsupported combination instead of drawing a false contour.
    if (context.tessellation.enabled ||
        std::none_of(
            context.mainView.opaqueItems.begin(),
            context.mainView.opaqueItems.end(),
            ShouldDrawOutline))
    {
        // 没有绘制也必须显式传递阶段结果；不能让后处理误用未完成的颜色身份。
        context.Resources().Publish(PassResourceId::OutlinedColor);
        context.Resources().Publish(PassResourceId::OutlinedIndirect);
        return;
    }

    RHIScopedPipelineState stateScope(m_Device.Commands());
    auto state=stateScope.saved; state.depthTest=true; state.depthCompare=RHICompare::Less;
    state.depthWrite=false; state.blend=false; state.cull=RHICull::Front;
    context.Resources().BeginTarget(PassResourceId::OutlinedColor);
    m_Device.Commands().SetPipelineState(state);

    m_Shader.Bind();
    m_Shader.SetMatrix4(
        "projection", &context.mainView.projection.cell[0]);

    for (const RenderItem& item : context.mainView.opaqueItems)
    {
        if (!ShouldDrawOutline(item))
            continue;

        const MaterialProperties& properties = item.material->GetProperties();
        const cy::Matrix4f modelView = context.mainView.view * item.model;
        const cy::Matrix3f normalMatrix =
            modelView.GetSubMatrix3().GetInverse().GetTranspose();

        RenderSubmissionStats::Get().RecordMaterialBind(item.material);
        m_Shader.SetMatrix4("modelView", &modelView.cell[0]);
        m_Shader.SetMatrix3("normalMatrix", &normalMatrix.cell[0]);
        m_Shader.SetFloat("outlineThickness", properties.outlineThickness);
        m_Shader.SetVec3(
            "outlineColor",
            properties.outlineColor.x,
            properties.outlineColor.y,
            properties.outlineColor.z);
        item.material->BindCoverage(m_Shader, 0);
        item.mesh->Draw();
    }

    context.Resources().EndTarget(PassResourceId::OutlinedColor, false);
    context.Resources().Publish(PassResourceId::OutlinedColor);
    context.Resources().Publish(PassResourceId::OutlinedIndirect);
}
