// SPDX-License-Identifier: MIT
/// @file ShadowPass.cpp
/// @brief 阴影渲染阶段的实现文件
/// @details 该文件实现了 ShadowPass 类的核心功能，包括初始化阴影贴图、加载着色器、执行阴影渲染流程等。
/// @author MaiX
/// @date 2026-08-11


#include "ShadowPass.h"

#include <algorithm>

#include "Renderer/Resources/Material.h"
#include "Renderer/Resources/Mesh.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/View/RenderView.h"

bool ShadowPass::Init(unsigned int width, unsigned int height)
{
    const bool shadersLoaded = ReloadShaders();
    const bool instanceBufferInitialized = m_InstanceBuffer.Init();
    m_ShadowMap.Init(width, height);
    return shadersLoaded && instanceBufferInitialized;
}

bool ShadowPass::ReloadShaders()
{
    const bool standardLoaded = m_StandardShader.Load(
        "assets/shaders/shadow/shadow_depth.vert",
        "assets/shaders/shadow/shadow_depth.frag");
    const bool instancedStandardLoaded = m_InstancedStandardShader.Load(
        "assets/shaders/shadow/shadow_depth_instanced.vert",
        "assets/shaders/shadow/shadow_depth.frag");
    const bool tessellationLoaded = m_TessellationShader.LoadTessellation(
        "assets/shaders/pbr/tessellation/pbr_tess.vert",
        "assets/shaders/pbr/tessellation/pbr_tess.tesc",
        "assets/shaders/shadow/shadow_tess.tese",
        "assets/shaders/shadow/shadow_depth.frag");
    const bool instanceBlockBound = instancedStandardLoaded &&
        m_InstanceBuffer.BindShaderBlock(m_InstancedStandardShader);
    return standardLoaded && instanceBlockBound && tessellationLoaded;
}

void ShadowPass::Execute(RenderPassContext& context)
{
    context.shadowTexture = m_ShadowMap.GetDepthTexture();
    if (!context.frame.shadowsEnabled)
        return;

    m_ShadowMap.BindForWriting();
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    if (context.tessellation.enabled)
    {
        Shader& shader = m_TessellationShader;
        shader.Bind();
        glPatchParameteri(GL_PATCH_VERTICES, 3);
        shader.SetFloat("tessellationLevel", context.tessellation.level);
        shader.SetFloat(
            "displacementScale", context.tessellation.displacementScale);
        for (const RenderItem& item : context.shadowView.opaqueItems)
        {
            if (!item.castsShadow || item.mesh == nullptr ||
                item.material == nullptr)
                continue;

            const cy::Matrix4f lightMvp =
                context.frame.lightVP * item.model;
            shader.SetMatrix4("lightMvp", &lightMvp.cell[0]);
            RenderSubmissionStats::Get().RecordMaterialBind(item.material);
            item.material->BindDisplacement(shader, 0);
            item.mesh->DrawPatches();
        }
    }
    else
    {
        m_BatchInstanceCounts.assign(
            context.shadowView.opaqueBatches.size(), 0);
        std::size_t uploadChunkCount = 0;
        for (std::size_t batchIndex = 0;
            batchIndex < context.shadowView.opaqueBatches.size();
            ++batchIndex)
        {
            const OpaqueRenderBatch& batch =
                context.shadowView.opaqueBatches[batchIndex];
            if (batch.itemCount == 0 ||
                batch.firstItem >= context.shadowView.opaqueItems.size())
                continue;
            if (context.shadowView.opaqueItems[batch.firstItem].mesh == nullptr)
                continue;

            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                context.shadowView.opaqueItems.size());
            for (std::size_t itemIndex = batch.firstItem;
                itemIndex < endItem;
                ++itemIndex)
            {
                if (context.shadowView.opaqueItems[itemIndex].castsShadow)
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
            batchIndex < context.shadowView.opaqueBatches.size();
            ++batchIndex)
        {
            const std::size_t instanceCount =
                m_BatchInstanceCounts[batchIndex];
            if (instanceCount <= 1)
                continue;

            const OpaqueRenderBatch& batch =
                context.shadowView.opaqueBatches[batchIndex];
            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                context.shadowView.opaqueItems.size());
            std::size_t instanceIndex = 0;
            for (std::size_t itemIndex = batch.firstItem;
                itemIndex < endItem;
                ++itemIndex)
            {
                const RenderItem& item =
                    context.shadowView.opaqueItems[itemIndex];
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
                    context.frame.lightVP * item.model;
                ++instanceIndex;
            }
            buildChunkIndex +=
                (instanceCount + InstanceBuffer::MaxInstancesPerDraw - 1) /
                InstanceBuffer::MaxInstancesPerDraw;
        }

        if (uploadChunkCount > 0)
        {
            m_InstanceBuffer.UploadChunks(
                m_InstanceTransforms.data(), uploadChunkCount);
        }

        Shader* activeShader = nullptr;
        std::size_t drawChunkIndex = 0;
        for (std::size_t batchIndex = 0;
            batchIndex < context.shadowView.opaqueBatches.size();
            ++batchIndex)
        {
            const OpaqueRenderBatch& batch =
                context.shadowView.opaqueBatches[batchIndex];
            if (batch.itemCount == 0 ||
                batch.firstItem >= context.shadowView.opaqueItems.size())
                continue;

            const RenderItem& firstItem =
                context.shadowView.opaqueItems[batch.firstItem];
            if (firstItem.mesh == nullptr)
                continue;

            const std::size_t instanceCount =
                m_BatchInstanceCounts[batchIndex];
            if (instanceCount == 0)
                continue;
            if (instanceCount > 1)
            {
                if (activeShader != &m_InstancedStandardShader)
                {
                    activeShader = &m_InstancedStandardShader;
                    activeShader->Bind();
                }
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

            if (activeShader != &m_StandardShader)
            {
                activeShader = &m_StandardShader;
                activeShader->Bind();
            }
            const std::size_t endItem = std::min(
                batch.firstItem + batch.itemCount,
                context.shadowView.opaqueItems.size());
            const RenderItem* shadowCaster = nullptr;
            for (std::size_t itemIndex = batch.firstItem;
                itemIndex < endItem;
                ++itemIndex)
            {
                if (context.shadowView.opaqueItems[itemIndex].castsShadow)
                {
                    shadowCaster =
                        &context.shadowView.opaqueItems[itemIndex];
                    break;
                }
            }
            if (shadowCaster == nullptr)
                continue;
            const cy::Matrix4f lightMvp =
                context.frame.lightVP * shadowCaster->model;
            activeShader->SetMatrix4("lightMvp", &lightMvp.cell[0]);
            firstItem.mesh->Draw();
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    m_ShadowMap.Unbind();
}
