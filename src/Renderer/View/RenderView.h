// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Renderer/Scene/LightSceneProxy.h"
#include "Renderer/View/RenderItem.h"
#include "Renderer/View/RenderBatch.h"
#include "Renderer/View/Frustum.h"

enum class RenderViewType
{
    Main,
    Reflection,
    Shadow
};

struct RenderView
{
    RenderViewType type = RenderViewType::Main;
    cy::Matrix4f view = cy::Matrix4f::Identity();
    cy::Matrix4f projection = cy::Matrix4f::Identity();
    cy::Matrix4f viewProjection = cy::Matrix4f::Identity();
    cy::Vec3f cameraWorldPosition{ 0.0f, 0.0f, 0.0f };
    unsigned int viewportWidth = 0;
    unsigned int viewportHeight = 0;
    Frustum frustum;
    bool frustumCullingEnabled = true;

    size_t sourcePrimitiveCount = 0;
    size_t visiblePrimitiveCount = 0;
    size_t culledPrimitiveCount = 0;
    size_t opaqueDrawCount = 0;
    size_t opaqueShaderGroupCount = 0;
    size_t opaqueMaterialGroupCount = 0;
    size_t opaqueMeshGroupCount = 0;
    size_t opaqueBatchCount = 0;

    std::vector<RenderItem> opaqueItems;
    std::vector<OpaqueRenderBatch> opaqueBatches;
    std::vector<RenderItem> translucentItems;
    std::vector<LightSceneProxy> lights;
};
