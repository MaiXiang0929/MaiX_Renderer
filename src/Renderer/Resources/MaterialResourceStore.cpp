// SPDX-License-Identifier: MIT
#include "MaterialResourceStore.h"
#include <algorithm>
#include <type_traits>

static_assert(std::is_nothrow_move_assignable<Material>::value,
              "Atomic material refresh requires non-throwing commit.");
static_assert(std::is_nothrow_move_assignable<MaterialInstance>::value,
              "Atomic override refresh requires non-throwing commit.");
MaterialResourceStore::~MaterialResourceStore()
{
    // 实例先释放纹理引用，再释放基础材质；父依赖仅在表存活期间有效。
    for (auto& entry : m_Resources) if (entry && entry->instance) entry.reset();
}
const MaterialResource* MaterialResourceStore::Get(MaterialHandle h) const
{
    return h.IsValid() && h.id < m_Resources.size() ? m_Resources[h.id].get() : nullptr;
}
MaterialHandle MaterialResourceStore::Create(Material material)
{
    if (m_Resources.size() >= InvalidRenderResourceId) return {};
    MaterialHandle h{static_cast<RenderResourceId>(m_Resources.size())};
    if (material.GetName().empty()) material.SetName("Material " + std::to_string(h.id));
    material.GetProperties() = SanitizeMaterialProperties(material.GetProperties());
    auto entry = std::make_unique<MaterialResource>();
    entry->effective = std::move(material);
    m_Resources.push_back(std::move(entry));
    return h;
}
MaterialHandle MaterialResourceStore::CreateInstance(MaterialHandle parent, std::string name)
{
    const auto* base = Get(parent);
    if (!base || base->instance || m_Resources.size() >= InvalidRenderResourceId) return {};
    MaterialHandle h{static_cast<RenderResourceId>(m_Resources.size())};
    if (name.empty()) name = base->effective.GetName() + " Instance " + std::to_string(h.id);
    auto entry = std::make_unique<MaterialResource>();
    entry->instance.emplace();
    entry->instance->parent = parent;
    entry->effective = entry->instance->Resolve(base->effective, name);
    m_Resources.push_back(std::move(entry));
    return h;
}
bool MaterialResourceStore::Destroy(MaterialHandle h)
{
    if (!Get(h)) return false;
    for (const auto& entry : m_Resources)
        if (entry && entry->instance && entry->instance->parent.id == h.id) return false;
    m_Resources[h.id].reset();
    return true;
}
bool MaterialResourceStore::CommitBase(MaterialHandle h, Material material)
{
    const auto* entry = Get(h);
    if (!entry || entry->instance) return false;
    // 先构建全部候选值；名称/纹理标签分配失败时，父与子都保留原状态。
    std::vector<std::pair<MaterialResource*, Material>> updates;
    for (auto& child : m_Resources)
        if (child && child->instance && child->instance->parent.id == h.id)
            updates.emplace_back(child.get(), child->instance->Resolve(material, child->effective.GetName()));
    m_Resources[h.id]->effective = std::move(material);
    for (auto& update : updates) update.first->effective = std::move(update.second);
    return true;
}
bool MaterialResourceStore::CommitInstance(MaterialHandle h, MaterialInstance instance)
{
    const auto* entry = Get(h);
    const auto* base = Get(instance.parent);
    if (!entry || !entry->instance || !base || base->instance) return false;
    Material resolved = instance.Resolve(base->effective, entry->effective.GetName());
    m_Resources[h.id]->instance = std::move(instance);
    m_Resources[h.id]->effective = std::move(resolved);
    return true;
}
bool MaterialResourceStore::UpdateBase(MaterialHandle h, const MaterialProperties& properties, BlendMode blend)
{
    const auto* entry = Get(h);
    if (!entry || entry->instance) return false;
    Material candidate = entry->effective;
    candidate.GetProperties() = SanitizeMaterialProperties(properties);
    candidate.SetBlendMode(blend);
    return CommitBase(h, std::move(candidate));
}
bool MaterialResourceStore::UpdateOverride(MaterialHandle h, MaterialParameter parameter,
                                          const MaterialProperties& values, bool enabled)
{
    const auto* entry = Get(h);
    const auto index = static_cast<std::size_t>(parameter);
    if (!entry || !entry->instance || index >= MaterialParameterCount) return false;
    auto candidate = *entry->instance;
    // 只写当前字段；不能把 UI 的整份有效快照复制成其它覆盖字段的值。
    switch (parameter)
    {
    case MaterialParameter::BaseColor: candidate.values.baseColor = values.baseColor; break;
    case MaterialParameter::SpecularColor: candidate.values.specularColor = values.specularColor; break;
    case MaterialParameter::Shininess: candidate.values.shininess = values.shininess; break;
    case MaterialParameter::EnvironmentReflectivity: candidate.values.environmentReflectivity = values.environmentReflectivity; break;
    case MaterialParameter::Metallic: candidate.values.metallic = values.metallic; break;
    case MaterialParameter::Roughness: candidate.values.roughness = values.roughness; break;
    case MaterialParameter::AmbientOcclusion: candidate.values.ambientOcclusion = values.ambientOcclusion; break;
    case MaterialParameter::NormalScale: candidate.values.normalScale = values.normalScale; break;
    case MaterialParameter::Opacity: candidate.values.opacity = values.opacity; break;
    case MaterialParameter::ToonThreshold: candidate.values.toonThreshold = values.toonThreshold; break;
    case MaterialParameter::ToonShadowStrength: candidate.values.toonShadowStrength = values.toonShadowStrength; break;
    case MaterialParameter::ToonShadowColor: candidate.values.toonShadowColor = values.toonShadowColor; break;
    case MaterialParameter::RimLightStrength: candidate.values.rimLightStrength = values.rimLightStrength; break;
    case MaterialParameter::RimLightColor: candidate.values.rimLightColor = values.rimLightColor; break;
    case MaterialParameter::FaceShadowEnabled: candidate.values.faceShadowEnabled = values.faceShadowEnabled; break;
    case MaterialParameter::FaceFrame: candidate.values.faceForwardLocal = values.faceForwardLocal; candidate.values.faceRightLocal = values.faceRightLocal; break;
    case MaterialParameter::FaceShadowSoftness: candidate.values.faceShadowSoftness = values.faceShadowSoftness; break;
    case MaterialParameter::FaceShadowMirrorX: candidate.values.faceShadowMirrorX = values.faceShadowMirrorX; break;
    case MaterialParameter::OutlineEnabled: candidate.values.outlineEnabled = values.outlineEnabled; break;
    case MaterialParameter::OutlineThickness: candidate.values.outlineThickness = values.outlineThickness; break;
    case MaterialParameter::OutlineColor: candidate.values.outlineColor = values.outlineColor; break;
    case MaterialParameter::Count: return false;
    }
    candidate.overrides.set(index, enabled);
    return CommitInstance(h, std::move(candidate));
}
bool MaterialResourceStore::ResetOverrides(MaterialHandle h)
{
    const auto* entry = Get(h);
    if (!entry || !entry->instance) return false;
    MaterialInstance candidate;
    candidate.parent = entry->instance->parent;
    return CommitInstance(h, std::move(candidate));
}
bool MaterialResourceStore::UpdateTexture(MaterialHandle h, MaterialTextureSlot slot,
                                         std::shared_ptr<Texture2D> texture, std::string source)
{
    const auto* entry = Get(h);
    const auto index = ToIndex(slot);
    if (!entry || index >= MaterialTextureSlotCount || !texture || !texture->IsValid() ||
        texture->GetColorSpace() != GetRequiredMaterialTextureColorSpace(slot)) return false;
    if (entry->instance)
    {
        auto candidate = *entry->instance;
        candidate.textures[index] = {TextureOverrideMode::Replace, std::move(texture), std::move(source)};
        return CommitInstance(h, std::move(candidate));
    }
    Material candidate = entry->effective;
    if (!candidate.SetTexture(slot, std::move(texture), std::move(source))) return false;
    return CommitBase(h, std::move(candidate));
}
bool MaterialResourceStore::ClearTexture(MaterialHandle h, MaterialTextureSlot slot)
{
    const auto* entry = Get(h);
    if (!entry || ToIndex(slot) >= MaterialTextureSlotCount) return false;
    if (entry->instance)
    {
        auto candidate = *entry->instance;
        candidate.textures[ToIndex(slot)] = {TextureOverrideMode::Disabled, {}, {}};
        return CommitInstance(h, std::move(candidate));
    }
    Material candidate = entry->effective;
    candidate.ClearTexture(slot);
    return CommitBase(h, std::move(candidate));
}
bool MaterialResourceStore::ResetTexture(MaterialHandle h, MaterialTextureSlot slot)
{
    const auto* entry = Get(h);
    if (!entry || !entry->instance || ToIndex(slot) >= MaterialTextureSlotCount) return false;
    auto candidate = *entry->instance;
    candidate.textures[ToIndex(slot)] = {};
    return CommitInstance(h, std::move(candidate));
}
std::vector<MaterialHandle> MaterialResourceStore::Handles() const
{
    std::vector<MaterialHandle> result;
    for (std::size_t i = 0; i < m_Resources.size(); ++i)
        if (m_Resources[i]) result.push_back({static_cast<RenderResourceId>(i)});
    return result;
}
std::size_t MaterialResourceStore::Count() const
{
    return static_cast<std::size_t>(std::count_if(m_Resources.begin(), m_Resources.end(),
        [](const auto& entry) { return entry != nullptr; }));
}
