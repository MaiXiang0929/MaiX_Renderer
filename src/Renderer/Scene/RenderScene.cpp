// SPDX-License-Identifier: MIT
#include "RenderScene.h"

#include <algorithm>
#include <tuple>

#include "Renderer/View/RenderView.h"

namespace
{
void SortOpaqueItems(RenderView& view)
{
    if (view.type == RenderViewType::Shadow)
    {
        std::stable_sort(
            view.opaqueItems.begin(),
            view.opaqueItems.end(),
            [](const RenderItem& left, const RenderItem& right)
            {
                return std::tie(
                    left.shaderId,
                    left.meshId,
                    left.materialId,
                    left.primitiveId) <
                    std::tie(
                        right.shaderId,
                        right.meshId,
                        right.materialId,
                        right.primitiveId);
            });
        return;
    }

    std::stable_sort(
        view.opaqueItems.begin(),
        view.opaqueItems.end(),
        [](const RenderItem& left, const RenderItem& right)
        {
            return std::tie(
                left.shaderId,
                left.materialId,
                left.meshId,
                left.primitiveId) <
                std::tie(
                    right.shaderId,
                    right.materialId,
                    right.meshId,
                    right.primitiveId);
        });
}

void SortTranslucentItems(RenderView& view)
{
    if (view.type == RenderViewType::Shadow)
        return;

    // OpenGL 观察空间中相机朝向 -Z，数值更小的物体离相机更远。
    // 稳定排序会保留同深度物体的场景提交顺序，避免逐帧顺序抖动。
    std::stable_sort(
        view.translucentItems.begin(),
        view.translucentItems.end(),
        [](const RenderItem& left, const RenderItem& right)
        {
            return left.sortDepth < right.sortDepth;
        });
}

void UpdateOpaqueStats(RenderView& view)
{
    view.opaqueDrawCount = view.opaqueItems.size();
    view.opaqueShaderGroupCount = 0;
    view.opaqueMaterialGroupCount = 0;
    view.opaqueMeshGroupCount = 0;
    view.opaqueBatchCount = 0;

    const RenderItem* previous = nullptr;
    for (const RenderItem& item : view.opaqueItems)
    {
        const bool shaderChanged =
            previous == nullptr || previous->shaderId != item.shaderId;
        if (shaderChanged)
            ++view.opaqueShaderGroupCount;
        if (shaderChanged || previous->materialId != item.materialId)
            ++view.opaqueMaterialGroupCount;
        if (previous == nullptr || previous->meshId != item.meshId)
            ++view.opaqueMeshGroupCount;
        previous = &item;
    }
}
}

PrimitiveId RenderScene::AddPrimitive(PrimitiveSceneProxy proxy)
{
    proxy.id = m_NextPrimitiveId++;
    m_Primitives.push_back(proxy);
    return proxy.id;
}

bool RenderScene::RemovePrimitive(PrimitiveId id)
{
    const auto iterator = std::find_if(
        m_Primitives.begin(),
        m_Primitives.end(),
        [id](const PrimitiveSceneProxy& proxy) { return proxy.id == id; });
    if (iterator == m_Primitives.end())
        return false;

    m_Primitives.erase(iterator);
    return true;
}

bool RenderScene::UpdatePrimitiveTransform(
    PrimitiveId id,
    const cy::Matrix4f& localToWorld)
{
    const auto iterator = std::find_if(
        m_Primitives.begin(),
        m_Primitives.end(),
        [id](const PrimitiveSceneProxy& proxy) { return proxy.id == id; });
    if (iterator == m_Primitives.end())
        return false;

    iterator->localToWorld = localToWorld;
    return true;
}

void RenderScene::UpdateMaterialBlendMode(
    RenderResourceId materialId,
    BlendMode blendMode)
{
    for (PrimitiveSceneProxy& proxy : m_Primitives)
    {
        if (proxy.materialId == materialId)
            proxy.blendMode = blendMode;
    }
}

LightId RenderScene::AddLight(LightSceneProxy light)
{
    light.id = m_NextLightId++;
    m_Lights.push_back(light);
    return light.id;
}

bool RenderScene::UpdateLight(LightId id, const LightSceneProxy& light)
{
    const auto iterator = std::find_if(
        m_Lights.begin(),
        m_Lights.end(),
        [id](const LightSceneProxy& proxy) { return proxy.id == id; });
    if (iterator == m_Lights.end())
        return false;

    const LightId preservedId = iterator->id;
    *iterator = light;
    iterator->id = preservedId;
    return true;
}

bool RenderScene::RemoveLight(LightId id)
{
    const auto iterator = std::find_if(
        m_Lights.begin(),
        m_Lights.end(),
        [id](const LightSceneProxy& proxy) { return proxy.id == id; });
    if (iterator == m_Lights.end())
        return false;

    m_Lights.erase(iterator);
    return true;
}

void RenderScene::BuildRenderView(RenderView& view) const
{
    view.opaqueItems.clear();
    view.translucentItems.clear();
    view.lights.clear();
    view.sourcePrimitiveCount = m_Primitives.size();
    view.visiblePrimitiveCount = 0;
    view.culledPrimitiveCount = 0;
    view.opaqueDrawCount = 0;
    view.opaqueShaderGroupCount = 0;
    view.opaqueMaterialGroupCount = 0;
    view.opaqueMeshGroupCount = 0;

    view.opaqueItems.reserve(m_Primitives.size());
    view.translucentItems.reserve(m_Primitives.size());
    for (const PrimitiveSceneProxy& proxy : m_Primitives)
    {
        if (!proxy.visible || proxy.mesh == nullptr || proxy.material == nullptr)
            continue;

        const PrimitiveBounds worldBounds = TransformBounds(
            proxy.localBounds, proxy.localToWorld);
        if (view.frustumCullingEnabled &&
            !view.frustum.IntersectsSphere(
                worldBounds.center, worldBounds.radius))
        {
            ++view.culledPrimitiveCount;
            continue;
        }

        RenderItem item{
            proxy.id,
            proxy.mesh,
            proxy.material,
            proxy.shaderId,
            proxy.materialId,
            proxy.meshId,
            proxy.localToWorld,
            0.0f,
            proxy.castsShadow
        };
        if (proxy.blendMode == BlendMode::AlphaBlend)
        {
            const cy::Vec4f centerView = view.view * cy::Vec4f(
                worldBounds.center.x,
                worldBounds.center.y,
                worldBounds.center.z,
                1.0f);
            item.sortDepth = centerView.z;
            view.translucentItems.push_back(item);
        }
        else
            view.opaqueItems.push_back(item);
        ++view.visiblePrimitiveCount;
    }

    SortOpaqueItems(view);
    SortTranslucentItems(view);
    UpdateOpaqueStats(view);
    view.opaqueBatches = BuildOpaqueRenderBatches(view.opaqueItems);
    view.opaqueBatchCount = view.opaqueBatches.size();

    view.lights = m_Lights;
}
