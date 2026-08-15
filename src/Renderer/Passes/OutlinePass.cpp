// SPDX-License-Identifier: MIT
#include "OutlinePass.h"

#include <algorithm>

#include "ForwardPass.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Resources/Material.h"
#include "Renderer/Resources/Mesh.h"
#include "Renderer/View/RenderView.h"

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
        "assets/shaders/npr/outline.vert",
        "assets/shaders/npr/outline.frag");
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
        return;

    const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depthWriteEnabled = GL_TRUE;
    GLint depthFunction = GL_LESS;
    GLint cullFaceMode = GL_BACK;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
    glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);

    m_ForwardPass.BindColorTarget();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

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
        item.mesh->Draw();
    }

    glCullFace(cullFaceMode);
    if (!cullFaceEnabled)
        glDisable(GL_CULL_FACE);
    if (blendEnabled)
        glEnable(GL_BLEND);
    glDepthMask(depthWriteEnabled);
    glDepthFunc(depthFunction);
    if (!depthTestEnabled)
        glDisable(GL_DEPTH_TEST);
    m_ForwardPass.UnbindColorTarget(false);
}
