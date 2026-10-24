// SPDX-License-Identifier: MIT
/// @file ShadowPass.cpp
/// @brief 阴影渲染阶段的实现文件
/// @details 该文件实现了 ShadowPass 类的核心功能，包括初始化阴影贴图、加载着色器、执行阴影渲染流程等。
/// @author MaiX
/// @date 2026-08-11


#include "Engine/Rendering/Passes/ShadowPass.h"

#include <algorithm>

#include "Engine/Rendering/Material/Material.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Engine/Rendering/Renderer/View/RenderView.h"

bool ShadowPass::Init(unsigned int width, unsigned int height)
{
    const bool shadersLoaded = ReloadShaders();
    const bool instanceBufferInitialized = m_InstanceBuffer.Init();
    (void)width; (void)height;
    return shadersLoaded && instanceBufferInitialized;
}

bool ShadowPass::ReloadShaders()
{
    const bool standardLoaded = m_StandardShader.Load(
        "Shaders/shadow/shadow_depth.vert",
        "Shaders/shadow/shadow_depth.frag");
    const bool instancedStandardLoaded = m_InstancedStandardShader.Load(
        "Shaders/shadow/shadow_depth_instanced.vert",
        "Shaders/shadow/shadow_depth.frag");
    const bool tessellationLoaded = m_TessellationShader.LoadTessellation(
        "Shaders/pbr/tessellation/pbr_tess.vert",
        "Shaders/pbr/tessellation/pbr_tess.tesc",
        "Shaders/shadow/shadow_tess.tese",
        "Shaders/shadow/shadow_depth.frag");
    const bool instanceBlockBound = instancedStandardLoaded &&
        m_InstanceBuffer.BindShaderBlock(m_InstancedStandardShader);
    const bool pointLoaded = m_PointShader.Load("Shaders/shadow/shadow_depth.vert", "Shaders/shadow/point_depth.frag");
    const bool pointInstancedLoaded = m_PointInstancedShader.Load("Shaders/shadow/shadow_depth_instanced.vert", "Shaders/shadow/point_depth.frag");
    const bool pointTessLoaded = m_PointTessellationShader.LoadTessellation(
        "Shaders/pbr/tessellation/pbr_tess.vert", "Shaders/pbr/tessellation/pbr_tess.tesc",
        "Shaders/shadow/shadow_tess.tese", "Shaders/shadow/point_depth.frag");
    const bool pointBlock = pointInstancedLoaded && m_InstanceBuffer.BindShaderBlock(m_PointInstancedShader);
    return standardLoaded && instanceBlockBound && tessellationLoaded && pointLoaded && pointTessLoaded && pointBlock;
}

ShadowPass::~ShadowPass()
{
    m_Device.DestroyTarget(m_ArrayTarget.handle);
    m_Device.DestroyTarget(m_CubeTarget.handle);
}

bool ShadowPass::PrepareTargets(const ShadowFrame& frame)
{
    const unsigned layers = std::max(frame.layers2D, 1u);
    const unsigned cubeLayers = std::max(frame.cubeCount, 1u) * 6;
    const bool arrayChange = !m_ArrayTarget.handle || m_Resolution != frame.resolution || m_Layers < layers;
    const bool cubeChange = !m_CubeTarget.handle || m_PointResolution != frame.pointResolution || m_CubeLayers < cubeLayers;
    RHITarget candidateArray, candidateCube;
    try
    {
        auto create = [&](unsigned size, unsigned count, RHITextureKind kind) {
            RHITargetDesc desc;
            desc.width = desc.height = size;
            desc.color = false; desc.shadow = true;
            desc.depthKind = kind; desc.layers = count;
            auto target = m_Device.CreateTarget(desc);
            if (!target.handle) throw RHIError("Shadow array allocation failed");
            return target;
        };
        if (arrayChange) candidateArray = create(frame.resolution, layers, RHITextureKind::Texture2DArray);
        if (cubeChange) candidateCube = create(frame.pointResolution, cubeLayers, RHITextureKind::CubemapArray);
        if (arrayChange)
        {
            m_Device.RemapTargetBindings(m_ArrayTarget.handle, candidateArray.handle);
            m_Device.DestroyTarget(m_ArrayTarget.handle); m_ArrayTarget = candidateArray;
            candidateArray = {}; m_Resolution = frame.resolution; m_Layers = layers;
        }
        if (cubeChange)
        {
            m_Device.RemapTargetBindings(m_CubeTarget.handle, candidateCube.handle);
            m_Device.DestroyTarget(m_CubeTarget.handle); m_CubeTarget = candidateCube;
            candidateCube = {}; m_PointResolution = frame.pointResolution; m_CubeLayers = cubeLayers;
        }
        return true;
    }
    catch (const RHIError&)
    {
        m_Device.DestroyTarget(candidateArray.handle); m_Device.DestroyTarget(candidateCube.handle);
        return false;
    }
}

void ShadowPass::Execute(RenderPassContext& context)
{
    if (!context.frame.shadowsEnabled) return;
    if (!context.shadowViews || context.shadowViews->size() != context.frame.shadowFrame.views.size())
        throw RHIError("Shadow view list does not match frame plan");
    auto& access = context.Resources();
    for (std::size_t i = 0; i < context.frame.shadowFrame.views.size(); ++i)
    {
        const auto& view = context.frame.shadowFrame.views[i];
        if (view.lightIndex >= context.frame.shadowFrame.lights.size()) throw RHIError("Invalid shadow light index");
        const auto id = view.point ? PassResourceId::ShadowCubeDepth : PassResourceId::ShadowDepth;
        access.BeginLayer(id, view.layer);
        RenderDepth(context, (*context.shadowViews)[i], view);
        access.EndLayer(id);
    }
    // Both sampler types remain valid even when this frame uses only one family.
    if (!context.frame.shadowFrame.layers2D) { access.BeginLayer(PassResourceId::ShadowDepth,0); access.EndLayer(PassResourceId::ShadowDepth); }
    if (!context.frame.shadowFrame.cubeCount)
        for (unsigned face = 0; face < 6; ++face) { access.BeginLayer(PassResourceId::ShadowCubeDepth,face); access.EndLayer(PassResourceId::ShadowCubeDepth); }
    access.Publish(PassResourceId::ShadowDepth);
    access.Publish(PassResourceId::ShadowCubeDepth);
}

void ShadowPass::RenderDepth(RenderPassContext& context, const RenderView& view, const ShadowViewFrame& shadowView)
{

    RHIScopedPipelineState stateScope(m_Device.Commands());
    auto state=stateScope.saved; state.offsetFill=true; state.offsetFactor=2; state.offsetUnits=4;
    m_Device.Commands().SetPipelineState(state);
    const auto& light = context.frame.shadowFrame.lights[shadowView.lightIndex];
    const auto preparePoint = [&](Shader& shader) {
        if (!shadowView.point) return;
        const auto inverse = shadowView.viewProjection.GetInverse();
        shader.SetMatrix4("inverseLightVP", inverse.cell);
        shader.SetVec3("shadowPosition", light.position.x, light.position.y, light.position.z);
        shader.SetFloat("shadowNear", light.nearPlane);
        shader.SetFloat("shadowFar", light.farPlane);
    };

    if (context.tessellation.enabled)
    {
        Shader& shader = shadowView.point ? m_PointTessellationShader : m_TessellationShader;
        shader.Bind();
        preparePoint(shader);
        shader.SetFloat("tessellationLevel", context.tessellation.level);
        shader.SetFloat(
            "displacementScale", context.tessellation.displacementScale);
        for (const RenderItem& item : view.opaqueItems)
        {
            if (!item.castsShadow || item.mesh == nullptr ||
                item.material == nullptr)
                continue;

            const cy::Matrix4f lightMvp =
                shadowView.viewProjection * item.model;
            shader.SetMatrix4("lightMvp", &lightMvp.cell[0]);
            RenderSubmissionStats::Get().RecordMaterialBind(item.material);
            item.material->BindCoverage(shader, 0);
            item.material->BindDisplacement(shader, 3);
            item.mesh->DrawPatches();
        }
    }
    else
    {
        m_BatchInstanceCounts.assign(
            view.opaqueBatches.size(), 0);
        std::size_t uploadChunkCount = 0;
        for (std::size_t batchIndex = 0;
            batchIndex < view.opaqueBatches.size();
            ++batchIndex)
        {
            const OpaqueRenderBatch& batch =
                view.opaqueBatches[batchIndex];
            if (batch.itemCount == 0 ||
                batch.firstItem >= view.opaqueItems.size())
                continue;
            if (view.opaqueItems[batch.firstItem].mesh == nullptr)
                continue;

            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                view.opaqueItems.size());
            for (std::size_t itemIndex = batch.firstItem;
                itemIndex < endItem;
                ++itemIndex)
            {
                if (view.opaqueItems[itemIndex].castsShadow)
                    ++m_BatchInstanceCounts[batchIndex];
            }
            const std::size_t instanceCount =
                m_BatchInstanceCounts[batchIndex];
            if (instanceCount > 1)
            {
                uploadChunkCount +=
                    (instanceCount + InstanceBuffer::MaxInstancesPerDraw - 1) /
                    InstanceBuffer::MaxInstancesPerDraw;
            }
        }

        m_InstanceTransforms.assign(
            uploadChunkCount * InstanceBuffer::MaxInstancesPerDraw,
            InstanceTransformData{});
        std::size_t buildChunkIndex = 0;
        for (std::size_t batchIndex = 0;
            batchIndex < view.opaqueBatches.size();
            ++batchIndex)
        {
            const std::size_t instanceCount =
                m_BatchInstanceCounts[batchIndex];
            if (instanceCount <= 1)
                continue;

            const OpaqueRenderBatch& batch =
                view.opaqueBatches[batchIndex];
            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                view.opaqueItems.size());
            std::size_t instanceIndex = 0;
            for (std::size_t itemIndex = batch.firstItem;
                itemIndex < endItem;
                ++itemIndex)
            {
                const RenderItem& item =
                    view.opaqueItems[itemIndex];
                if (!item.castsShadow)
                    continue;

                const std::size_t chunkIndex =
                    buildChunkIndex +
                    instanceIndex / InstanceBuffer::MaxInstancesPerDraw;
                const std::size_t indexInChunk =
                    instanceIndex % InstanceBuffer::MaxInstancesPerDraw;
                m_InstanceTransforms[
                    chunkIndex * InstanceBuffer::MaxInstancesPerDraw +
                    indexInChunk].modelView =
                    shadowView.viewProjection * item.model;
                ++instanceIndex;
            }
            buildChunkIndex +=
                (instanceCount + InstanceBuffer::MaxInstancesPerDraw - 1) /
                InstanceBuffer::MaxInstancesPerDraw;
        }

        if (uploadChunkCount > 0)
        {
            if (!m_InstanceBuffer.UploadChunks(
                m_InstanceTransforms.data(), uploadChunkCount)) throw RHIError("Shadow instance upload failed");
        }

        Shader* activeShader = nullptr;
        std::size_t drawChunkIndex = 0;
        for (std::size_t batchIndex = 0;
            batchIndex < view.opaqueBatches.size();
            ++batchIndex)
        {
            const OpaqueRenderBatch& batch =
                view.opaqueBatches[batchIndex];
            if (batch.itemCount == 0 ||
                batch.firstItem >= view.opaqueItems.size())
                continue;

            const RenderItem& firstItem =
                view.opaqueItems[batch.firstItem];
            if (firstItem.mesh == nullptr)
                continue;

            const std::size_t instanceCount =
                m_BatchInstanceCounts[batchIndex];
            if (instanceCount == 0)
                continue;
            if (instanceCount > 1)
            {
                if (activeShader != (shadowView.point ? &m_PointInstancedShader : &m_InstancedStandardShader))
                {
                    activeShader = (shadowView.point ? &m_PointInstancedShader : &m_InstancedStandardShader);
                    activeShader->Bind();
                    preparePoint(*activeShader);
                }
                firstItem.material->BindCoverage(*activeShader, 0);
                for (std::size_t firstInstance = 0;
                    firstInstance < instanceCount;
                    firstInstance += InstanceBuffer::MaxInstancesPerDraw)
                {
                    const std::size_t drawInstanceCount = std::min(
                        InstanceBuffer::MaxInstancesPerDraw,
                        instanceCount - firstInstance);
                    m_InstanceBuffer.BindChunk(drawChunkIndex);
                    firstItem.mesh->DrawInstanced(drawInstanceCount);
                    ++drawChunkIndex;
                }
                continue;
            }

            if (activeShader != (shadowView.point ? &m_PointShader : &m_StandardShader))
            {
                activeShader = (shadowView.point ? &m_PointShader : &m_StandardShader);
                activeShader->Bind();
                preparePoint(*activeShader);
            }
            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                view.opaqueItems.size());
            const RenderItem* shadowCaster = nullptr;
            for (std::size_t itemIndex = batch.firstItem;
                itemIndex < endItem;
                ++itemIndex)
            {
                if (view.opaqueItems[itemIndex].castsShadow)
                {
                    shadowCaster =
                        &view.opaqueItems[itemIndex];
                    break;
                }
            }
            if (shadowCaster == nullptr)
                continue;
            const cy::Matrix4f lightMvp =
                shadowView.viewProjection * shadowCaster->model;
            activeShader->SetMatrix4("lightMvp", &lightMvp.cell[0]);
            shadowCaster->material->BindCoverage(*activeShader, 0);
            firstItem.mesh->Draw();
        }
    }


}
