// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/ForwardPass.h"

#include <algorithm>
#include <iostream>

#include "Engine/Rendering/Renderer/Resources/CubemapTexture.h"
#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Material/Material.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"
#include "Engine/Rendering/Renderer/View/RenderView.h"

namespace
{
void BindEnvironment(RenderPassContext& context, unsigned int unit)
{
    const RHITextureHandle texture = context.Resources().Texture(PassResourceId::Environment);
    context.device.Commands().BindTexture(texture,unit,RHITextureKind::Cubemap);
}

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
    m_Device.DestroyBuffer(m_LightBuffer);
}

bool ForwardPass::Init()
{
    const bool loaded = ReloadShaders();
    const bool instanceBufferInitialized = m_InstanceBuffer.Init();
    m_LightBuffer=m_Device.CreateBuffer({RHIBufferUsage::Uniform,sizeof(GpuLightData)*MaxForwardLights,true});
    return loaded && instanceBufferInitialized && m_LightBuffer != 0;
}

bool ForwardPass::Resize(unsigned int width, unsigned int height)
{
    FramebufferSpecification specification;
    specification.width = static_cast<int>(width);
    specification.height = static_cast<int>(height);
    specification.colorFormat = FramebufferColorFormat::RGBA16F;
    specification.sampleableDepth = true;
    specification.secondaryColorEnabled = true;
    return m_Framebuffer.Init(specification);
}

bool ForwardPass::ReloadShaders()
{
    const bool standardLoaded = m_StandardShader.Load(
        "Shaders/pbr/pbr.vert",
        "Shaders/pbr/pbr.frag");
    const bool instancedStandardLoaded = m_InstancedStandardShader.Load(
        "Shaders/pbr/pbr_instanced.vert",
        "Shaders/pbr/pbr.frag");
    const bool tessellationLoaded = m_TessellationShader.LoadTessellation(
        "Shaders/pbr/tessellation/pbr_tess.vert",
        "Shaders/pbr/tessellation/pbr_tess.tesc",
        "Shaders/pbr/tessellation/pbr_tess.tese",
        "Shaders/pbr/pbr.frag");
    const bool skyboxLoaded = m_SkyboxShader.Load(
        "Shaders/environment/skybox.vert",
        "Shaders/environment/skybox.frag");
    const bool groundLoaded = m_GroundShader.Load(
        "Shaders/reflection/ground.vert",
        "Shaders/reflection/ground.frag");
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
    return m_Device.BindUniformBlock(shader.GetProgramID(),"ForwardLights",ForwardLightsBindingPoint);
}

LightUploadData ForwardPass::UploadLights(
    const RenderView& view,
    LightId shadowLightId,
    LightId keyLightId)
{
    const LightUploadData upload = BuildLightUploadData(
        view.lights, view.view, shadowLightId, keyLightId);

    m_Device.UpdateBuffer(m_LightBuffer,0,sizeof(upload.lights),upload.lights.data());
    m_Device.Commands().BindUniformBuffer(m_LightBuffer,ForwardLightsBindingPoint,0,sizeof(upload.lights));

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
    context.Resources().BeginTarget(PassResourceId::ForwardColor);
    const RHITextureHandle reflectionTexture = context.Resources().Texture(PassResourceId::ReflectionColor);
    const RHITextureHandle shadowTexture = context.frame.shadowsEnabled
        ? context.Resources().Texture(PassResourceId::ShadowDepth) : 0;

    RenderSkybox(context, context.frame.view);

    if (context.frame.reflectionGroundEnabled)
    {
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
        BindEnvironment(context, 0);
        m_GroundShader.SetInt("cubemap", 0);
        m_Device.Commands().BindTexture(reflectionTexture,1);
        m_GroundShader.SetInt("reflectionTex", 1);
        m_Device.Commands().BindTexture(shadowTexture,2);
        m_GroundShader.SetInt("shadowMap", 2);
        m_GroundShader.SetInt(
            "shadowsEnabled", context.frame.shadowsEnabled ? 1 : 0);
        const auto shadowLight = std::find_if(context.mainView.lights.begin(), context.mainView.lights.end(),
            [&](const LightSceneProxy& light) { return light.id == context.frame.shadowLightId; });
        m_GroundShader.SetInt("shadowIsDirectional", shadowLight != context.mainView.lights.end() &&
            shadowLight->type == LightType::Directional ? 1 : 0);
        context.groundMesh.Draw();
    }

    RenderSurface(context, context.mainView);

    context.Resources().EndTarget(PassResourceId::ForwardColor, false);
    context.Resources().Publish(PassResourceId::ForwardColor);
    context.Resources().Publish(PassResourceId::ForwardDepth);
    context.Resources().Publish(PassResourceId::ForwardIndirect);
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

        item.mesh->DrawPatches();
        if (!allowWireframeOverlay || !context.tessellation.wireframe ||
            renderView.type == RenderViewType::Reflection)
            continue;

        shader.SetInt("wireframePass", 1);
        {
            RHIScopedPipelineState stateScope(m_Device.Commands());
            auto state=stateScope.saved; state.offsetLine=true; state.offsetFactor=-1; state.offsetUnits=-1; state.wireframe=true;
            m_Device.Commands().SetPipelineState(state);
            item.mesh->DrawPatches();
        }
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
        renderView,
        context.frame.shadowLightId,
        context.frame.keyLightId);
    shader.SetInt("lightCount", static_cast<int>(lightUpload.lightCount));
    shader.SetInt("shadowLightIndex", lightUpload.shadowLightIndex);
    shader.SetInt("keyLightIndex", lightUpload.keyLightIndex);

    const float viewToWorld[9] = {
        view.cell[0], view.cell[4], view.cell[8],
        view.cell[1], view.cell[5], view.cell[9],
        view.cell[2], view.cell[6], view.cell[10]
    };
    shader.SetMatrix3("viewToWorld",viewToWorld);

    if (tessellationEnabled)
    {
        shader.SetFloat("tessellationLevel", context.tessellation.level);
        shader.SetFloat(
            "displacementScale", context.tessellation.displacementScale);
    }
    shader.SetInt("wireframePass", 0);

    m_Device.Commands().BindTexture(context.Resources().Texture(PassResourceId::Irradiance),4,RHITextureKind::Cubemap);
    shader.SetInt("irradianceMap",4);
    m_Device.Commands().BindTexture(context.Resources().Texture(PassResourceId::PrefilteredEnvironment),7,RHITextureKind::Cubemap);
    shader.SetInt("prefilterMap",7);
    m_Device.Commands().BindTexture(context.Resources().Texture(PassResourceId::BrdfLut),8);
    shader.SetInt("brdfLut",8);
    shader.SetFloat("prefilterMaxLod",static_cast<float>(EnvironmentLighting::PrefilterMaxMip));
    const RHITextureHandle shadowTexture = context.frame.shadowsEnabled
        ? context.Resources().Texture(PassResourceId::ShadowDepth) : 0;
    m_Device.Commands().BindTexture(shadowTexture,5);
    shader.SetInt("shadowMap", 5);
    shader.SetInt(
        "shadowsEnabled", context.frame.shadowsEnabled ? 1 : 0);

}

void ForwardPass::RenderSkybox(
    RenderPassContext& context,
    const cy::Matrix4f& view)
{
    if (context.Resources().Texture(PassResourceId::Environment) == 0)
        return;

    cy::Matrix4f skyboxView = view;
    skyboxView.cell[12] = 0.0f;
    skyboxView.cell[13] = 0.0f;
    skyboxView.cell[14] = 0.0f;

    m_SkyboxShader.Bind();
    m_SkyboxShader.SetMatrix4(
        "projection", &context.frame.projection.cell[0]);
    m_SkyboxShader.SetMatrix4("view", &skyboxView.cell[0]);
    BindEnvironment(context, 0);
    m_SkyboxShader.SetInt("skybox", 0);

    RHIScopedPipelineState stateScope(m_Device.Commands());
    auto state=stateScope.saved; state.depthWrite=false; state.depthCompare=RHICompare::LessEqual;
    m_Device.Commands().SetPipelineState(state);
    context.skyboxMesh.Draw();
}
