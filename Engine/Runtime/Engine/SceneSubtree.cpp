// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/SceneSubtree.h"
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <algorithm>
#include <stdexcept>

struct SceneSubtreeArchive
{
    World::SubtreeSnapshot world;
    std::vector<std::pair<std::size_t,SceneModel>> models;
    std::vector<std::pair<std::size_t,SceneLight>> lights;
    std::vector<std::uint64_t> modelOrder;
    std::vector<LightId> lightOrder;
    std::optional<SceneCamera> camera;
    std::vector<std::pair<std::size_t,MaterialHandle>> sceneMaterials;
    Renderer::SceneResourceSnapshot resources;
    Renderer::RetentionId retention=0;
    Renderer* renderer=nullptr;
    std::weak_ptr<const int> lifetime;
    ModelId defaultModel;
    LightId mainLight=InvalidLightId;
    bool detached=true;
};
namespace
{
template<class T,class Id,class GetId> void InsertOrdered(std::vector<T>& values,T value,std::size_t index,const std::vector<Id>& order,GetId getId)
{
    auto position=values.end();
    for(std::size_t i=index+1;i<order.size();++i) {
        position=std::find_if(values.begin(),values.end(),[&](const auto& item) { return getId(item)==order[i]; });
        if(position!=values.end()) break;
    }
    if(position==values.end()) {
        bool anchored=false;
        for(std::size_t i=std::min(index,order.size());i>0;--i) {
            const auto before=std::find_if(values.begin(),values.end(),[&](const auto& item) { return getId(item)==order[i-1]; });
            if(before!=values.end()) { position=before+1; anchored=true; break; }
        }
        if(!anchored) position=values.begin()+std::min(index,values.size());
    }
    values.insert(position,std::move(value));
}
template<class T> void Append(std::vector<T>& values,T handle)
{
    if (std::none_of(values.begin(),values.end(),[&](T value){return value.id==handle.id;})) values.push_back(handle);
}
bool DependsOn(const Renderer& renderer,MaterialHandle value,MaterialHandle handle)
{
    for (;value.IsValid();value=renderer.GetMaterialParent(value)) if (value.id==handle.id) return true;
    return false;
}
bool ModelsOwnMaterial(const std::vector<SceneModel>& models,const Renderer& renderer,MaterialHandle handle)
{
    for (const auto& model:models)
    {
        for (auto value:model.materials) if (DependsOn(renderer,value,handle)) return true;
        for (const auto& section:model.sections) if (DependsOn(renderer,section.material,handle)) return true;
    }
    return false;
}
bool OwnsMaterial(const SceneState& scene,const Renderer& renderer,MaterialHandle handle)
{
    // Persistent definitions remain active even when only unloaded regions reference them.
    for(const auto& value:scene.contentMaterials)
        if(value.region==scene.contentWorld&&DependsOn(renderer,value.handle,handle)) return true;
    for (auto value:scene.sceneMaterials) if (DependsOn(renderer,value,handle)) return true;
    return ModelsOwnMaterial(scene.models,renderer,handle);
}
bool OwnsMesh(const SceneState& scene,MeshHandle handle)
{
    for (const auto& model:scene.models) for (auto mesh:model.meshes) if (mesh.id==handle.id) return true;
    return false;
}
}
std::vector<MaterialHandle> GetSceneMaterialHandles(const SceneState& scene,const Renderer& renderer)
{
    auto materials=renderer.GetMaterialHandles();
    materials.erase(std::remove_if(materials.begin(),materials.end(),[&](MaterialHandle handle) {
        // In world mode Renderer also owns private upload and retirement resources.
        // Editor selection/capture may enumerate only active scene ownership.
        return (renderer.IsMaterialRetained(handle)||!scene.contentWorld.empty())&&!OwnsMaterial(scene,renderer,handle);
    }),materials.end());
    return materials;
}
void ReleaseUnusedModelMaterial(SceneState& scene,Renderer& renderer,ModelId owner,MaterialHandle material)
{
    auto* model=FindSceneModel(scene.models,owner);
    if (!model || !renderer.IsMaterialInstance(material) ||
        std::any_of(model->sections.begin(),model->sections.end(),[&](const auto& section){return section.material.id==material.id;})) return;
    // Active bookkeeping follows the successful binding change; GPU retention has a different lifetime.
    model->materials.erase(std::remove_if(model->materials.begin(),model->materials.end(),
        [&](auto value){return value.id==material.id;}),model->materials.end());
    // Persistent material definitions can be referenced by unloaded region documents.
    if (!scene.contentWorld.empty()&&std::any_of(scene.contentMaterials.begin(),scene.contentMaterials.end(),[&](const auto& m) {
        return m.handle.id==material.id&&m.region==scene.contentWorld; })) return;
    if (!ModelsOwnMaterial(scene.models,renderer,material))
        scene.sceneMaterials.erase(std::remove_if(scene.sceneMaterials.begin(),scene.sceneMaterials.end(),
            [&](auto value){return value.id==material.id;}),scene.sceneMaterials.end());
    if (!OwnsMaterial(scene,renderer,material)) renderer.DestroyMaterial(material);
}
bool DetachSceneSubtree(SceneState& scene,Renderer& renderer,ActorId root,
                       std::shared_ptr<SceneSubtreeArchive>& output,std::string& error)
{
    std::shared_ptr<SceneSubtreeArchive> archive;
    try
    {
        if (output) { error="Archive output must be empty."; return false; }
        archive=std::make_shared<SceneSubtreeArchive>();
        for(const auto& model:scene.models) archive->modelOrder.push_back(model.id.value);
        for(const auto& light:scene.lights) archive->lightOrder.push_back(light.proxy.id);
        if (!scene.world.CaptureSubtree(root,archive->world,error)) return false;
        auto contains=[&](ActorId id) {
            const auto& entries=archive->world.Entries();
            return std::any_of(entries.begin(),entries.end(),[&](const auto& entry){return entry.actor.id==id;});
        };
        World world;
        if (!scene.world.PrepareSubtree(archive->world,false,world,error)) return false;
        auto models=scene.models; auto lights=scene.lights;
        auto sceneMaterials=scene.sceneMaterials;
        std::vector<MaterialHandle> boundMaterials;
        for (std::size_t i=0;i<scene.models.size();++i)
            if (contains(scene.models[i].actor))
            {
                const auto& model=scene.models[i]; archive->models.emplace_back(i,model);
                for (auto mesh:model.meshes) Append(archive->resources.meshes,mesh);
                for (auto material:model.materials) Append(archive->resources.materials,material);
                for (const auto& section:model.sections)
                {
                    MeshHandle mesh; MaterialHandle material; bool castsShadow;
                    if (!renderer.GetPrimitiveBinding(section.primitiveId,mesh,material,castsShadow) || material.id!=section.material.id)
                        throw std::runtime_error("Subtree primitive binding is unavailable or stale.");
                    archive->resources.primitives.push_back(section.primitiveId);
                    Append(archive->resources.meshes,mesh); Append(archive->resources.materials,material);
                    Append(boundMaterials,material);
                }
                if (scene.defaultModelId==model.id) archive->defaultModel=model.id;
            }
        for (std::size_t i=0;i<scene.lights.size();++i)
            if (contains(scene.lights[i].actor))
            {
                archive->lights.emplace_back(i,scene.lights[i]);
                archive->resources.lights.push_back(scene.lights[i].proxy.id);
                if (scene.mainLightId==scene.lights[i].proxy.id) archive->mainLight=scene.mainLightId;
            }
        if (contains(scene.camera.actor)) archive->camera=scene.camera;
        for (const auto& entry:archive->world.Entries())
        {
            const auto& actor=entry.actor;
            if (actor.kind==ActorKind::Model && std::none_of(archive->models.begin(),archive->models.end(),[&](const auto& m){return m.second.actor==actor.id && m.second.id.value==actor.componentId;}))
                throw std::runtime_error("Subtree model component is missing.");
            if (actor.kind==ActorKind::Light && std::none_of(archive->lights.begin(),archive->lights.end(),[&](const auto& l){return l.second.actor==actor.id && l.second.proxy.id==actor.componentId;}))
                throw std::runtime_error("Subtree light component is missing.");
            if (actor.kind==ActorKind::Camera && (!archive->camera || archive->camera->actor!=actor.id || archive->camera->id.value!=actor.componentId))
                throw std::runtime_error("Subtree camera component is missing.");
        }
        for (std::size_t i=0;i<archive->resources.materials.size();++i)
        {
            const auto parent=renderer.GetMaterialParent(archive->resources.materials[i]);
            if (parent.IsValid()) Append(archive->resources.materials,parent);
        }
        models.erase(std::remove_if(models.begin(),models.end(),[&](const auto& m){return contains(m.actor);}),models.end());
        lights.erase(std::remove_if(lights.begin(),lights.end(),[&](const auto& l){return contains(l.actor);}),lights.end());
        for (std::size_t i=0;i<scene.sceneMaterials.size();++i)
        {
            const auto handle=scene.sceneMaterials[i];
            const bool worldOwned=!scene.contentWorld.empty()&&std::any_of(scene.contentMaterials.begin(),scene.contentMaterials.end(),[&](const auto& m) {
                return m.handle.id==handle.id&&m.region==scene.contentWorld; });
            const bool usedByDeleted=std::any_of(boundMaterials.begin(),boundMaterials.end(),[&](auto value){return DependsOn(renderer,value,handle);});
            if (!worldOwned && usedByDeleted && !ModelsOwnMaterial(models,renderer,handle)) archive->sceneMaterials.emplace_back(i,handle);
        }
        sceneMaterials.erase(std::remove_if(sceneMaterials.begin(),sceneMaterials.end(),[&](auto handle){
            return std::any_of(archive->sceneMaterials.begin(),archive->sceneMaterials.end(),[&](const auto& entry){return entry.second.id==handle.id;});
        }),sceneMaterials.end());
        // All allocating CPU preparation precedes publication of any scene mutation.
        archive->renderer=&renderer; archive->lifetime=renderer.Lifetime();
        archive->retention=renderer.RetainSceneResources(archive->resources,error);
        if (!archive->retention) return false;
        if (!renderer.SetRetainedSceneActive(archive->retention,false,error))
        { renderer.ReleaseRetainedScene(archive->retention); return false; }
        scene.world=std::move(world); scene.models.swap(models); scene.lights.swap(lights);
        scene.sceneMaterials.swap(sceneMaterials);
        if (archive->camera) scene.camera=SceneCamera{};
        if (archive->defaultModel.IsValid()) scene.defaultModelId={};
        if (archive->mainLight!=InvalidLightId) scene.mainLightId=InvalidLightId;
        output=std::move(archive); error.clear(); return true;
    }
    catch (const std::exception& exception)
    {
        if (archive && archive->retention) renderer.ReleaseRetainedScene(archive->retention);
        error=exception.what(); return false;
    }
}
bool RestoreSceneSubtree(SceneState& scene,Renderer& renderer,SceneSubtreeArchive& archive,std::string& error)
{
    try
    {
        if (!archive.detached || archive.renderer!=&renderer || archive.lifetime.expired())
        { error="Subtree archive is inactive or belongs to another renderer."; return false; }
        World world;
        if (!scene.world.PrepareSubtree(archive.world,true,world,error)) return false;
        auto models=scene.models; auto lights=scene.lights;
        auto sceneMaterials=scene.sceneMaterials;
        auto camera=scene.camera;
        for (const auto& entry:archive.models)
        {
            if (FindSceneModel(models,entry.second.id)) { error="Model identity conflicts."; return false; }
            auto model=entry.second; const auto* actor=world.Find(model.actor);
            model.name=actor->name; model.transform=actor->world; model.visible=world.IsVisible(actor->id);
            InsertOrdered(models,std::move(model),entry.first,archive.modelOrder,[](const SceneModel& m) { return m.id.value; });
        }
        for (const auto& entry:archive.lights)
        {
            if (FindSceneLight(lights,entry.second.proxy.id)) { error="Light identity conflicts."; return false; }
            auto light=entry.second; const auto* actor=world.Find(light.actor);
            light.name=actor->name; light.transform=actor->world; light.proxy.visible=world.IsVisible(actor->id);
            InsertOrdered(lights,std::move(light),entry.first,archive.lightOrder,[](const SceneLight& l) { return l.proxy.id; });
        }
        if (archive.camera)
        {
            if (camera.id.IsValid()) { error="Scene camera identity conflicts."; return false; }
            camera=*archive.camera; const auto* actor=world.Find(camera.actor);
            camera.name=actor->name; camera.transform=actor->world; camera.visible=world.IsVisible(actor->id);
        }
        for (const auto& entry:archive.sceneMaterials)
            if (std::none_of(sceneMaterials.begin(),sceneMaterials.end(),[&](auto value){return value.id==entry.second.id;}))
                sceneMaterials.insert(sceneMaterials.begin()+std::min(entry.first,sceneMaterials.size()),entry.second);
        if (!renderer.SetRetainedSceneActive(archive.retention,true,error)) return false;
        scene.world=std::move(world); scene.models.swap(models); scene.lights.swap(lights); scene.camera=std::move(camera);
        scene.sceneMaterials.swap(sceneMaterials);
        if (!scene.defaultModelId.IsValid()) scene.defaultModelId=archive.defaultModel;
        if (scene.mainLightId==InvalidLightId) scene.mainLightId=archive.mainLight;
        archive.detached=false;
        // These updates use existing IDs and contain no allocations or resource creation.
        for (const auto& entry:archive.models)
        {
            auto& model=*FindSceneModel(scene.models,entry.second.id);
            ApplySceneModelTransform(model,renderer);
            for (const auto& section:model.sections) renderer.UpdatePrimitiveVisibility(section.primitiveId,model.visible);
        }
        for (const auto& entry:archive.lights) ApplySceneLightTransform(*FindSceneLight(scene.lights,entry.second.proxy.id),renderer);
        error.clear(); return true;
    }
    catch (const std::exception& exception) { error=exception.what(); return false; }
}
void ReleaseSceneSubtree(SceneState& scene,Renderer& renderer,std::shared_ptr<SceneSubtreeArchive>& archive) noexcept
{
    if (!archive) return;
    if (archive->renderer==&renderer && !archive->lifetime.expired())
    {
        renderer.ReleaseRetainedScene(archive->retention);
        for (auto mesh:archive->resources.meshes) if (!OwnsMesh(scene,mesh)) renderer.DestroyMesh(mesh);
        for (int instances=1;instances>=0;--instances)
            for (auto material:archive->resources.materials)
                if (renderer.IsMaterialInstance(material)==(instances!=0) && !OwnsMaterial(scene,renderer,material))
                    renderer.DestroyMaterial(material);
    }
    archive.reset();
}
