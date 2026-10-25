// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Scene/RenderScene.h"
#include <unordered_set>

#include <algorithm>
#include <tuple>

#include "Engine/Rendering/Renderer/View/RenderView.h"
#include "Engine/Rendering/Material/Material.h"
#include "Engine/Rendering/Renderer/View/MaterialFacing.h"

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
                    left.reversedWinding,
                    left.primitiveId) <
                    std::tie(
                        right.shaderId,
                        right.meshId,
                        right.materialId,
                        right.reversedWinding,
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
                left.reversedWinding,
                left.primitiveId) <
                std::tie(
                    right.shaderId,
                    right.materialId,
                    right.meshId,
                    right.reversedWinding,
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

const PrimitiveSceneProxy* RenderScene::FindPrimitive(PrimitiveId id) const
{
    const auto found=std::find_if(m_Primitives.begin(),m_Primitives.end(),[&](const auto& p){return p.id==id;});
    return found==m_Primitives.end()?nullptr:&*found;
}
bool RenderScene::PrepareUpdate(const std::vector<PrimitiveSceneProxy>& addedPrimitives,
    const std::vector<LightSceneProxy>& addedLights,const std::vector<PrimitiveId>& removedPrimitives,
    const std::vector<LightId>& removedLights,RenderScene& result,std::string& error) const
{
    if (&result==this) { error="Render scene candidate aliases its input."; return false; }
    RenderScene candidate=*this;
    std::unordered_set<PrimitiveId> removedP;
    std::unordered_set<LightId> removedL;
    for (auto id:removedPrimitives) if (!removedP.insert(id).second||!candidate.RemovePrimitive(id)) { error="Missing/duplicate removal primitive."; return false; }
    for (auto id:removedLights) if (!removedL.insert(id).second||!candidate.RemoveLight(id)) { error="Missing/duplicate removal light."; return false; }
    std::unordered_set<PrimitiveId> primitives;
    std::unordered_set<LightId> lights;
    for (const auto& p:candidate.m_Primitives) primitives.insert(p.id);
    for (const auto& l:candidate.m_Lights) lights.insert(l.id);
    for (const auto& p:addedPrimitives) {
        if (p.id==InvalidPrimitiveId||!p.mesh||!p.material||!primitives.insert(p.id).second) { error="Invalid/duplicate staged primitive."; return false; }
        candidate.m_Primitives.push_back(p);
    }
    for (const auto& l:addedLights) {
        if (l.id==InvalidLightId||!lights.insert(l.id).second) { error="Invalid/duplicate staged light."; return false; }
        candidate.m_Lights.push_back(l);
    }
    result=std::move(candidate); error.clear(); return true;
}
bool RenderScene::CaptureBindings(const std::vector<PrimitiveId>& primitives, const std::vector<LightId>& lights,
                                  RetainedBindings& result, std::string& error) const
{
    RetainedBindings captured;
    for(const auto& primitive:m_Primitives) captured.primitiveOrder.push_back(primitive.id);
    for(const auto& light:m_Lights) captured.lightOrder.push_back(light.id);
    for (std::size_t i=0;i<m_Primitives.size();++i)
        if (std::find(primitives.begin(),primitives.end(),m_Primitives[i].id)!=primitives.end())
            captured.primitives.emplace_back(i,m_Primitives[i]);
    for (std::size_t i=0;i<m_Lights.size();++i)
        if (std::find(lights.begin(),lights.end(),m_Lights[i].id)!=lights.end())
            captured.lights.emplace_back(i,m_Lights[i]);
    if (captured.primitives.size()!=primitives.size() || captured.lights.size()!=lights.size())
    { error="Scene bindings are missing or duplicated."; return false; }
    result=std::move(captured); error.clear(); return true;
}
bool RenderScene::SetBindingsActive(const RetainedBindings& bindings, bool active, std::string& error)
{
    auto primitives=m_Primitives;
    auto lights=m_Lights;
    auto change=[&](auto& items,const auto& entries,const auto& order) {
        for (const auto& entry:entries)
        {
            const auto found=std::find_if(items.begin(),items.end(),[&](const auto& value) { return value.id==entry.second.id; });
            if (active)
            {
                if (found!=items.end()) return false;
                auto position=items.end();
                for(std::size_t i=entry.first+1;i<order.size();++i) {
                    position=std::find_if(items.begin(),items.end(),[&](const auto& value) { return value.id==order[i]; });
                    if(position!=items.end()) break;
                }
                if(position==items.end()) {
                    bool anchored=false;
                    for(std::size_t i=std::min(entry.first,order.size());i>0;--i) {
                        const auto before=std::find_if(items.begin(),items.end(),[&](const auto& value) { return value.id==order[i-1]; });
                        if(before!=items.end()) { position=before+1; anchored=true; break; }
                    }
                    if(!anchored) position=items.begin()+std::min(entry.first,items.size());
                }
                items.insert(position,entry.second);
            }
            else
            {
                if (found==items.end()) return false;
                items.erase(found);
            }
        }
        return true;
    };
    if (!change(primitives,bindings.primitives,bindings.primitiveOrder) || !change(lights,bindings.lights,bindings.lightOrder))
    { error="Retained scene bindings conflict with the active scene."; return false; }
    m_Primitives.swap(primitives); m_Lights.swap(lights); error.clear(); return true;
}
std::vector<PrimitiveId> RenderScene::PrimitiveIds() const
{
    std::vector<PrimitiveId> ids; ids.reserve(m_Primitives.size());
    for (const auto& p:m_Primitives) ids.push_back(p.id);
    return ids;
}
std::vector<LightId> RenderScene::LightIds() const
{
    std::vector<LightId> ids; ids.reserve(m_Lights.size());
    for (const auto& l:m_Lights) ids.push_back(l.id);
    return ids;
}
PrimitiveId RenderScene::AddPrimitive(PrimitiveSceneProxy proxy)
{
    proxy.id = ReservePrimitiveId();
    if (proxy.id==InvalidPrimitiveId) return InvalidPrimitiveId;
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

bool RenderScene::UpdatePrimitiveVisibility(PrimitiveId id, bool visible)
{
    for (auto& proxy:m_Primitives) if (proxy.id==id) { proxy.visible=visible; return true; }
    return false;
}

bool RenderScene::HasMeshReference(RenderResourceId meshId) const
{
    return std::any_of(
        m_Primitives.begin(),
        m_Primitives.end(),
        [meshId](const PrimitiveSceneProxy& proxy)
        {
            return proxy.meshId == meshId;
        });
}

bool RenderScene::HasMaterialReference(RenderResourceId materialId) const
{
    return std::any_of(
        m_Primitives.begin(),
        m_Primitives.end(),
        [materialId](const PrimitiveSceneProxy& proxy)
        {
            return proxy.materialId == materialId;
        });
}

bool RenderScene::ReplacePrimitiveMaterials(const std::vector<PrimitiveId>& ids,
    RenderResourceId expected, RenderResourceId replacement, const Material* material, BlendMode blend)
{
    if (ids.empty() || !material || replacement == InvalidRenderResourceId) return false;
    // 全部验证完成后才提交；不存在的 Primitive 或旧绑定不匹配不会造成部分替换。
    for (auto id : ids)
    {
        const auto found = std::find_if(m_Primitives.begin(), m_Primitives.end(),
            [id](const auto& proxy) { return proxy.id == id; });
        if (found == m_Primitives.end() || found->materialId != expected) return false;
    }
    for (auto& proxy : m_Primitives)
        if (std::find(ids.begin(), ids.end(), proxy.id) != ids.end())
        {
            proxy.material = material;
            proxy.materialId = replacement;
            proxy.blendMode = blend;
        }
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
    light.id = ReserveLightId();
    if (light.id==InvalidLightId) return InvalidLightId;
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
        item.reversedWinding = IsSurfaceWindingReversed(
            proxy.material->GetProperties().surfaceSide, view.view * proxy.localToWorld);
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

    for (const auto& light:m_Lights) if (light.visible) view.lights.push_back(light);
}
