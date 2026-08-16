// SPDX-License-Identifier: MIT
#include "ForwardPass.h"

#include <algorithm>
#include <iostream>

#include "Renderer/Resources/CubemapTexture.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Resources/Material.h"
#include "Renderer/Resources/Mesh.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Scene/LightSceneProxy.h"
#include "Renderer/View/RenderView.h"

namespace
{
InstanceTransformData BuildInstanceTransform(
    const cy::Matrix4f& view,
    const cy::Matrix4f& model)
{
    InstanceTransformData instance;
    instance.modelView = view * model;
    return instance;
}
}

ForwardPass::~ForwardPass()
{
    if (m_LightBuffer != 0)
        glDeleteBuffers(1, &m_LightBuffer);
}

bool ForwardPass::Init()
{
    const bool loaded = ReloadShaders();
    const bool instanceBufferInitialized = m_InstanceBuffer.Init();
    glGenBuffers(1, &m_LightBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_LightBuffer);
    glBufferData(
        GL_UNIFORM_BUFFER,
        sizeof(GpuLightData) * MaxForwardLights,
        nullptr,
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return loaded && instanceBufferInitialized && m_LightBuffer != 0;
}

bool ForwardPass::Resize(unsigned int width, unsigned int height)
{
    FramebufferSpecification specification;
    specification.width = static_cast<int>(width);
    specification.height = static_cast<int>(height);
    specification.colorFormat = FramebufferColorFormat::RGBA16F;
    specification.sampleableDepth = true;
    return m_Framebuffer.Init(specification);
}

bool ForwardPass::ReloadShaders()
{
    const bool standardLoaded = m_StandardShader.Load(
        "assets/shaders/pbr/pbr.vert",
        "assets/shaders/pbr/pbr.frag");
    const bool instancedStandardLoaded = m_InstancedStandardShader.Load(
        "assets/shaders/pbr/pbr_instanced.vert",
        "assets/shaders/pbr/pbr.frag");
    const bool tessellationLoaded = m_TessellationShader.LoadTessellation(
        "assets/shaders/pbr/tessellation/pbr_tess.vert",
        "assets/shaders/pbr/tessellation/pbr_tess.tesc",
        "assets/shaders/pbr/tessellation/pbr_tess.tese",
        "assets/shaders/pbr/pbr.frag");
    const bool skyboxLoaded = m_SkyboxShader.Load(
        "assets/shaders/environment/skybox.vert",
        "assets/shaders/environment/skybox.frag");
    const bool groundLoaded = m_GroundShader.Load(
        "assets/shaders/reflection/ground.vert",
        "assets/shaders/reflection/ground.frag");
    const bool standardBlockBound = standardLoaded &&
        BindForwardLightsBlock(m_StandardShader);
    const bool instancedStandardBlockBound = instancedStandardLoaded &&
        BindForwardLightsBlock(m_InstancedStandardShader) &&
        m_InstanceBuffer.BindShaderBlock(m_InstancedStandardShader);
    const bool tessellationBlockBound = tessellationLoaded &&
        BindForwardLightsBlock(m_TessellationShader);
    return standardBlockBound && instancedStandardBlockBound &&
        tessellationBlockBound && skyboxLoaded && groundLoaded;
}

bool ForwardPass::BindForwardLightsBlock(const Shader& shader) const
{
    const GLuint blockIndex = glGetUniformBlockIndex(
        shader.GetProgramID(), "ForwardLights");
    if (blockIndex == GL_INVALID_INDEX)
    {
        std::cerr
            << "[ForwardPass] ForwardLights uniform block was not found."
            << std::endl;
        return false;
    }

    glUniformBlockBinding(
        shader.GetProgramID(), blockIndex, ForwardLightsBindingPoint);
    return true;
}

LightUploadData ForwardPass::UploadLights(
    const RenderView& view,
    LightId shadowLightId)
{
    const LightUploadData upload = BuildLightUploadData(
        view.lights, view.view, shadowLightId);

    glBindBuffer(GL_UNIFORM_BUFFER, m_LightBuffer);
    glBufferSubData(
        GL_UNIFORM_BUFFER,
        0,
        sizeof(upload.lights),
        upload.lights.data());
    glBindBufferBase(
        GL_UNIFORM_BUFFER, ForwardLightsBindingPoint, m_LightBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    if (upload.truncated && !m_LightLimitWarningIssued)
    {
        std::cerr
            << "[ForwardPass] Forward light count exceeds "
            << MaxForwardLights << "; extra lights are ignored."
            << std::endl;
        m_LightLimitWarningIssued = true;
    }
    return upload;
}

void ForwardPass::Execute(RenderPassContext& context)
{
    // CPU 侧切换主颜色目标；随后所有绘制命令写入同一个 Framebuffer。
    m_Framebuffer.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderSkybox(context, context.frame.view);

    m_GroundShader.Bind();
    m_GroundShader.SetMatrix4("mvp", &context.frame.groundMvp.cell[0]);
    m_GroundShader.SetMatrix4("model", &context.frame.groundModel.cell[0]);
    m_GroundShader.SetMatrix4(
        "reflectionVP", &context.frame.reflectionVP.cell[0]);
    m_GroundShader.SetMatrix4("lightVP", &context.frame.lightVP.cell[0]);
    m_GroundShader.SetVec3(
        "cameraWorldPos",
        context.frame.cameraWorldPosition.x,
        context.frame.cameraWorldPosition.y,
        context.frame.cameraWorldPosition.z);
    context.cubemap.Bind(0);
    m_GroundShader.SetInt("cubemap", 0);
    glActiveTexture(GL_TEXTURE1);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 1, context.reflectionTexture);
    glBindTexture(GL_TEXTURE_2D, context.reflectionTexture);
    m_GroundShader.SetInt("reflectionTex", 1);
    glActiveTexture(GL_TEXTURE2);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 2, context.shadowTexture);
    glBindTexture(GL_TEXTURE_2D, context.shadowTexture);
    m_GroundShader.SetInt("shadowMap", 2);
    m_GroundShader.SetInt(
        "shadowsEnabled", context.frame.shadowsEnabled ? 1 : 0);
    context.groundMesh.Draw();

    RenderSurface(context, context.mainView);

    context.sceneColorTexture = GetColorTexture();

    m_Framebuffer.Unbind();
}

void ForwardPass::RenderSurface(
    RenderPassContext& context,
    RenderView& renderView)
{
    const bool hasInstancedBatch = std::any_of(
        renderView.opaqueBatches.begin(),
        renderView.opaqueBatches.end(),
        ShouldUseInstancing);
    if (!context.tessellation.enabled && hasInstancedBatch)
    {
        RenderOpaqueBatches(context, renderView);
        return;
    }
    RenderItems(context, renderView, renderView.opaqueItems, true);
}

void ForwardPass::RenderTranslucentSurface(
    RenderPassContext& context,
    RenderView& renderView)
{
    RenderItems(context, renderView, renderView.translucentItems, false);
}

void ForwardPass::RenderItems(
    RenderPassContext& context,
    RenderView& renderView,
    const std::vector<RenderItem>& items,
    bool allowWireframeOverlay)
{
    Shader& shader = context.tessellation.enabled
        ? m_TessellationShader
        : m_StandardShader;
    PrepareSurfaceShader(
        context, renderView, shader, context.tessellation.enabled);

    const cy::Matrix4f& view = renderView.view;
    const cy::Matrix4f& viewProjection = renderView.viewProjection;

    for (const RenderItem& item : items)
    {
        if (item.mesh == nullptr || item.material == nullptr)
            continue;

        const cy::Matrix4f mv = view * item.model;
        const cy::Matrix4f mvp = viewProjection * item.model;
        const cy::Matrix4f lightMvp = context.frame.lightVP * item.model;
        shader.SetMatrix4("mvp", &mvp.cell[0]);
        shader.SetMatrix4("mv", &mv.cell[0]);
        shader.SetMatrix4("lightMvp", &lightMvp.cell[0]);
        item.material->Bind(shader, 0);

        if (!context.tessellation.enabled)
        {
            item.mesh->Draw();
            continue;
        }

        glPatchParameteri(GL_PATCH_VERTICES, 3);
        item.mesh->DrawPatches();
        if (!allowWireframeOverlay || !context.tessellation.wireframe ||
            renderView.type == RenderViewType::Reflection)
            continue;

        shader.SetInt("wireframePass", 1);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        item.mesh->DrawPatches();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
        shader.SetInt("wireframePass", 0);
    }
}

void ForwardPass::RenderOpaqueBatches(
    RenderPassContext& context,
    RenderView& renderView)
{
    std::size_t uploadChunkCount = 0;
    for (const OpaqueRenderBatch& batch : renderView.opaqueBatches)
    {
        if (!ShouldUseInstancing(batch) ||
            batch.firstItem >= renderView.opaqueItems.size())
            continue;
        const RenderItem& firstItem =
            renderView.opaqueItems[batch.firstItem];
        if (firstItem.mesh == nullptr || firstItem.material == nullptr)
            continue;

        const std::size_t availableItemCount = std::min(
            batch.itemCount,
            renderView.opaqueItems.size() - batch.firstItem);
        uploadChunkCount +=
            (availableItemCount + InstanceBuffer::MaxInstancesPerDraw - 1) /
            InstanceBuffer::MaxInstancesPerDraw;
    }

    m_InstanceTransforms.assign(
        uploadChunkCount * InstanceBuffer::MaxInstancesPerDraw,
        InstanceTransformData{});
    std::size_t buildChunkIndex = 0;
    for (const OpaqueRenderBatch& batch : renderView.opaqueBatches)
    {
        if (!ShouldUseInstancing(batch) ||
            batch.firstItem >= renderView.opaqueItems.size())
            continue;
        const RenderItem& firstItem =
            renderView.opaqueItems[batch.firstItem];
        if (firstItem.mesh == nullptr || firstItem.material == nullptr)
            continue;

        const std::size_t endItem = std::min(
            batch.firstItem + batch.itemCount,
            renderView.opaqueItems.size());
        for (std::size_t firstInstance = batch.firstItem;
            firstInstance < endItem;
            firstInstance += InstanceBuffer::MaxInstancesPerDraw)
        {
            const std::size_t instanceCount = std::min(
                InstanceBuffer::MaxInstancesPerDraw,
                endItem - firstInstance);
            InstanceTransformData* destination =
                m_InstanceTransforms.data() +
                buildChunkIndex * InstanceBuffer::MaxInstancesPerDraw;
            for (std::size_t instanceIndex = 0;
                instanceIndex < instanceCount;
                ++instanceIndex)
            {
                destination[instanceIndex] = BuildInstanceTransform(
                    renderView.view,
                    renderView.opaqueItems[
                        firstInstance + instanceIndex].model);
            }
            ++buildChunkIndex;
        }
    }

    if (uploadChunkCount > 0 && !m_InstanceBuffer.UploadChunks(
            m_InstanceTransforms.data(), uploadChunkCount))
    {
        RenderItems(context, renderView, renderView.opaqueItems, true);
        return;
    }

    Shader* activeShader = nullptr;
    std::size_t drawChunkIndex = 0;
    for (const OpaqueRenderBatch& batch : renderView.opaqueBatches)
    {
        if (batch.itemCount == 0 ||
            batch.firstItem >= renderView.opaqueItems.size())
            continue;

        const RenderItem& firstItem =
            renderView.opaqueItems[batch.firstItem];
        if (firstItem.mesh == nullptr || firstItem.material == nullptr)
            continue;

        if (ShouldUseInstancing(batch))
        {
            if (activeShader != &m_InstancedStandardShader)
            {
                activeShader = &m_InstancedStandardShader;
                PrepareSurfaceShader(
                    context, renderView, *activeShader, false);
                const cy::Matrix4f viewToWorld =
                    renderView.view.GetInverse();
                const cy::Matrix4f projectionFromView =
                    renderView.viewProjection * viewToWorld;
                const cy::Matrix4f lightFromView =
                    context.frame.lightVP * viewToWorld;
                activeShader->SetMatrix4(
                    "projectionFromView", &projectionFromView.cell[0]);
                activeShader->SetMatrix4(
                    "lightFromView", &lightFromView.cell[0]);
            }

            firstItem.material->Bind(*activeShader, 0);
            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                renderView.opaqueItems.size());
            for (std::size_t firstInstance = batch.firstItem;
                firstInstance < endItem;
                firstInstance += InstanceBuffer::MaxInstancesPerDraw)
            {
                const std::size_t instanceCount = std::min(
                    InstanceBuffer::MaxInstancesPerDraw,
                    endItem - firstInstance);
                m_InstanceBuffer.BindChunk(drawChunkIndex);
                firstItem.mesh->DrawInstanced(instanceCount);
                ++drawChunkIndex;
            }
            continue;
        }

        if (activeShader != &m_StandardShader)
        {
            activeShader = &m_StandardShader;
            PrepareSurfaceShader(context, renderView, *activeShader, false);
        }
        const cy::Matrix4f mv = renderView.view * firstItem.model;
        const cy::Matrix4f mvp =
            renderView.viewProjection * firstItem.model;
        const cy::Matrix4f lightMvp =
            context.frame.lightVP * firstItem.model;
        activeShader->SetMatrix4("mvp", &mvp.cell[0]);
        activeShader->SetMatrix4("mv", &mv.cell[0]);
        activeShader->SetMatrix4("lightMvp", &lightMvp.cell[0]);
        firstItem.material->Bind(*activeShader, 0);
        firstItem.mesh->Draw();
    }
}

void ForwardPass::PrepareSurfaceShader(
    RenderPassContext& context,
    RenderView& renderView,
    Shader& shader,
    bool tessellationEnabled)
{
    const cy::Matrix4f& view = renderView.view;
    shader.Bind();
    const LightUploadData lightUpload = UploadLights(
        renderView, context.frame.shadowLightId);
    shader.SetInt("lightCount", static_cast<int>(lightUpload.lightCount));
    shader.SetInt("shadowLightIndex", lightUpload.shadowLightIndex);

    const float viewToWorld[9] = {
        view.cell[0], view.cell[4], view.cell[8],
        view.cell[1], view.cell[5], view.cell[9],
        view.cell[2], view.cell[6], view.cell[10]
    };
    const GLint viewToWorldLocation = glGetUniformLocation(
        shader.GetProgramID(), "viewToWorld");
    if (viewToWorldLocation != -1)
        glUniformMatrix3fv(viewToWorldLocation, 1, GL_FALSE, viewToWorld);

    if (tessellationEnabled)
    {
        shader.SetFloat("tessellationLevel", context.tessellation.level);
        shader.SetFloat(
            "displacementScale", context.tessellation.displacementScale);
    }
    shader.SetInt("wireframePass", 0);

    context.cubemap.Bind(4);
    shader.SetInt("cubemap", 4);
    glActiveTexture(GL_TEXTURE5);
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, 5, context.shadowTexture);
    glBindTexture(GL_TEXTURE_2D, context.shadowTexture);
    shader.SetInt("shadowMap", 5);
    shader.SetInt(
        "shadowsEnabled", context.frame.shadowsEnabled ? 1 : 0);

}

void ForwardPass::RenderSkybox(
    RenderPassContext& context,
    const cy::Matrix4f& view)
{
    if (!context.cubemap.IsValid())
        return;

    cy::Matrix4f skyboxView = view;
    skyboxView.cell[12] = 0.0f;
    skyboxView.cell[13] = 0.0f;
    skyboxView.cell[14] = 0.0f;

    m_SkyboxShader.Bind();
    m_SkyboxShader.SetMatrix4(
        "projection", &context.frame.projection.cell[0]);
    m_SkyboxShader.SetMatrix4("view", &skyboxView.cell[0]);
    context.cubemap.Bind(0);
    m_SkyboxShader.SetInt("skybox", 0);

    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    context.skyboxMesh.Draw();
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}
