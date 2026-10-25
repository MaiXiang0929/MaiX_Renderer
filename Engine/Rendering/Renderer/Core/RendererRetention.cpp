// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <algorithm>

Renderer::RetentionId Renderer::RetainSceneResources(const SceneResourceSnapshot& resources, std::string& error)
{
    auto retained=std::make_unique<RetainedScene>();
    retained->resources=resources;
    if (!m_RenderScene.CaptureBindings(resources.primitives,resources.lights,retained->bindings,error)) return 0;
    auto append=[](auto& values, auto handle) {
        if (std::none_of(values.begin(),values.end(),[&](auto value){return value.id==handle.id;})) values.push_back(handle);
    };
    for (const auto& entry:retained->bindings.primitives)
    {
        append(retained->resources.meshes,MeshHandle{entry.second.meshId});
        append(retained->resources.materials,MaterialHandle{entry.second.materialId});
    }
    for (std::size_t i=0;i<retained->resources.materials.size();++i)
    {
        const auto parent=GetMaterialParent(retained->resources.materials[i]);
        if (parent.IsValid()) append(retained->resources.materials,parent);
    }
    for (auto mesh:retained->resources.meshes)
        if (!mesh.IsValid() || mesh.id>=m_MeshResources.size() || !m_MeshResources[mesh.id])
        { error="Cannot retain an invalid mesh."; return 0; }
    for (auto material:retained->resources.materials)
        if (!m_MaterialResources.Get(material)) { error="Cannot retain an invalid material."; return 0; }
    m_RetainedScenes.push_back(std::move(retained));
    return m_RetainedScenes.size();
}
bool Renderer::SetRetainedSceneActive(RetentionId id, bool active, std::string& error)
{
    if (!id || id>m_RetainedScenes.size() || !m_RetainedScenes[id-1] || m_RetainedScenes[id-1]->active==active)
    { error="Retained scene identity/state is invalid."; return false; }
    auto& retained=*m_RetainedScenes[id-1];
    auto bindings=retained.bindings;
    if (active)
        for (auto& entry:bindings.primitives)
        {
            auto& proxy=entry.second;
            const auto* material=m_MaterialResources.Get({proxy.materialId});
            if (!material || proxy.meshId>=m_MeshResources.size() || !m_MeshResources[proxy.meshId])
            { error="Retained primitive resource is unavailable."; return false; }
            proxy.mesh=m_MeshResources[proxy.meshId].get();
            proxy.material=&material->effective;
            proxy.blendMode=material->effective.GetBlendMode();
        }
    if (!m_RenderScene.SetBindingsActive(bindings,active,error)) return false;
    retained.active=active; return true;
}
void Renderer::ReleaseRetainedScene(RetentionId id)
{
    if (id && id<=m_RetainedScenes.size()) m_RetainedScenes[id-1].reset();
}
bool Renderer::IsMeshRetained(MeshHandle handle) const
{
    for (const auto& retained:m_RetainedScenes)
        if (retained)
            for (auto mesh:retained->resources.meshes) if (mesh.id==handle.id) return true;
    return false;
}
bool Renderer::IsMaterialRetained(MaterialHandle handle) const
{
    for (const auto& retained:m_RetainedScenes)
        if (retained)
            for (auto material:retained->resources.materials) if (material.id==handle.id) return true;
    return false;
}
