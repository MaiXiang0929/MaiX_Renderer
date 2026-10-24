// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/ScenePersistence.h"
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Asset/Importer/FbxModelImporter.h"
#include "Engine/Asset/Importer/ImageLoader.h"
#include "Engine/Asset/Geometry/CubeGeometry.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace
{
void Require(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); }
void ReleaseSnapshot(const Renderer::SceneResourceSnapshot& resources,Renderer& renderer)
{
    for (auto p:resources.primitives) renderer.RemovePrimitive(p);
    for (auto l:resources.lights) renderer.RemoveLight(l);
    for (auto m:resources.meshes) renderer.DestroyMesh(m);
    for (int instance=1;instance>=0;--instance)
        for (auto m:resources.materials)
            if (renderer.IsMaterialInstance(m)==(instance!=0)) renderer.DestroyMaterial(m);
}
}

void DestroySceneResources(SceneState& scene,Renderer& renderer)
{
    // Remove every proxy before releasing any resource shared across models.
    for (auto& m:scene.models) for (const auto& s:m.sections) renderer.RemovePrimitive(s.primitiveId);
    for (auto& l:scene.lights) renderer.RemoveLight(l.proxy.id);
    for (auto& m:scene.models) for (auto h:m.meshes) renderer.DestroyMesh(h);
    for (int instance=1;instance>=0;--instance)
    {
        for (auto h:scene.sceneMaterials)
            if (renderer.IsMaterialInstance(h)==(instance!=0)) renderer.DestroyMaterial(h);
        for (auto& m:scene.models) for (auto h:m.materials)
            if (renderer.IsMaterialInstance(h)==(instance!=0)) renderer.DestroyMaterial(h);
    }
    scene=SceneState{};
}

bool CaptureSceneDocument(SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,
                          SceneDocument& output,std::string& error)
{
    try
    {
        Require(RegisterSceneActors(scene),"Cannot register scene actors before saving.");
        const auto resources=renderer.GetSceneResources();
        SceneDocument d;
        d.postProcess=scene.postProcess;
        d.shadowSettings=scene.shadowSettings;
        d.environment=scene.environment;
        if (!scene.environmentSource.empty())
        {
            Require(std::filesystem::is_regular_file(scene.environmentSource),"Environment source is missing.");
            d.environmentSource=MakeSceneAssetPath(scene.environmentSource,roots);
        }
        std::unordered_map<RenderResourceId,std::uint64_t> materials;
        std::unordered_map<std::uint64_t,std::uint64_t> actors;
        std::unordered_set<PrimitiveId> primitives;
        for (auto handle:resources.materials) materials.emplace(handle.id,materials.size()+1);
        for (const auto& actor:scene.world.Actors()) actors.emplace(actor.id.value,actors.size()+1);
        for (auto handle:resources.materials)
        {
            Renderer::MaterialSnapshot snapshot;
            Require(renderer.GetMaterialSnapshot(handle,snapshot),"Stale material while saving.");
            SceneMaterialRecord m; m.id=materials.at(handle.id); m.name=snapshot.name; m.blend=snapshot.blendMode;
            m.properties=snapshot.isInstance?snapshot.overrideValues:snapshot.properties;
            if (snapshot.isInstance)
            { m.parent=materials.at(snapshot.parent.id); m.overrides=snapshot.overrides; }
            for (size_t i=0;i<MaterialTextureSlotCount;++i)
            {
                auto& t=m.textures[i];
                t.mode=snapshot.isInstance?snapshot.textureModes[i]:TextureOverrideMode::Inherit;
                if (snapshot.hasTextures[i]&&(!snapshot.isInstance||t.mode==TextureOverrideMode::Replace))
                {
                    Require(!snapshot.textureFiles[i].empty(),"Texture has no reload source: "+snapshot.name+
                        " (generated textures are not supported by scene files).");
                    Require(std::filesystem::is_regular_file(snapshot.textureFiles[i]),"Missing texture source: "+snapshot.textureFiles[i].u8string());
                    t.source=MakeSceneAssetPath(snapshot.textureFiles[i],roots);
                    t.embeddedId=snapshot.embeddedTextureIds[i];
                }
            }
            d.materials.push_back(std::move(m));
        }
        size_t modelCount=0,lightCount=0,cameraCount=0;
        for (const auto& actor:scene.world.Actors())
        {
            SceneActorRecord a; a.id=actors.at(actor.id.value); a.parent=actor.parent.IsValid()?actors.at(actor.parent.value):0;
            a.name=actor.name; a.local=actor.local; a.visible=actor.visible;
            if (actor.kind==ActorKind::Model)
            {
                const auto it=std::find_if(scene.models.begin(),scene.models.end(),[&](const auto& m){return m.actor==actor.id;});
                Require(it!=scene.models.end(),"Model component is missing."); const auto& model=*it; ++modelCount;
                a.model.emplace(); auto& m=*a.model;
                if (!model.sourcePath.empty())
                {
                    Require(std::filesystem::is_regular_file(model.sourcePath),"Missing model source: "+model.sourcePath.u8string());
                    m.source=MakeSceneAssetPath(model.sourcePath,roots);
                }
                m.cubeEdge=model.sourceCubeEdge; m.meshNames=model.sourceMeshNames;
                Require(!m.meshNames.empty()&&m.meshNames.size()==model.meshes.size(),"Model has no supported reload source: "+a.name);
                for (const auto& section:model.sections)
                {
                    MeshHandle mesh; MaterialHandle material; bool shadow=true;
                    Require(renderer.GetPrimitiveBinding(section.primitiveId,mesh,material,shadow)&&primitives.insert(section.primitiveId).second,
                        "Invalid or duplicate section primitive.");
                    Require(material.id==section.material.id,"Section material binding is stale.");
                    const auto index=std::find_if(model.meshes.begin(),model.meshes.end(),[&](auto h){return h.id==mesh.id;});
                    Require(index!=model.meshes.end(),"Section mesh is not tracked by its model.");
                    SceneSectionRecord s; s.mesh=static_cast<size_t>(index-model.meshes.begin()); s.material=materials.at(material.id);
                    s.name=section.name; s.local=section.localTransform; s.bounds=section.localBounds; s.castsShadow=shadow;
                    m.sections.push_back(std::move(s));
                }
                if (model.id==scene.defaultModelId) d.defaultModel=a.id;
            }
            if (actor.kind==ActorKind::Light)
            {
                const auto it=std::find_if(scene.lights.begin(),scene.lights.end(),[&](const auto& l){return l.actor==actor.id;});
                Require(it!=scene.lights.end(),"Light component is missing."); ++lightCount; a.light=it->proxy;
                if (it->proxy.id==scene.mainLightId) d.mainLight=a.id;
            }
            if (actor.kind==ActorKind::Camera)
            {
                Require(scene.camera.actor==actor.id&&scene.camera.id.IsValid(),"Camera component is missing.");
                ++cameraCount; a.camera=scene.camera;
            }
            d.actors.push_back(std::move(a));
        }
        Require(modelCount==scene.models.size()&&lightCount==scene.lights.size()&&cameraCount==(scene.camera.id.IsValid()?1u:0u),"Unregistered typed scene components.");
        Require(primitives.size()==resources.primitives.size()&&lightCount==resources.lights.size(),
            "Scene contains renderer-only objects without reload provenance; saving would omit visible content.");
        d.reflectionGroundEnabled=scene.reflectionGroundEnabled; d.groundY=scene.groundY; d.modelDiameter=scene.modelDiameter;
        d.sceneRadius=scene.sceneRadius; d.defaultBoundsMin=scene.defaultBoundsMin; d.defaultBoundsMax=scene.defaultBoundsMax; d.objCenter=scene.objCenter;
        if (!ValidateSceneDocument(d,error)) return false;
        output=std::move(d); error.clear(); return true;
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}

bool ReplaceSceneFromDocument(SceneState& active,Renderer& renderer,const SceneAssetRoots& roots,
                              const SceneDocument& d,std::string& error)
{
    if (!ValidateSceneDocument(d,error)) return false;
    SceneState candidate;
    candidate.postProcess=d.postProcess;
    candidate.shadowSettings=d.shadowSettings;
    candidate.environment=d.environment;
    struct Rollback
    {
        SceneState& scene; Renderer& renderer;
        ~Rollback() { DestroySceneResources(scene,renderer); }
    } rollback{candidate,renderer};
    try
    {
        if (d.environmentSource) candidate.environmentSource=ResolveSceneAssetPath(*d.environmentSource,roots);
        const auto old=renderer.GetSceneResources();
        candidate.models.reserve(d.actors.size()); candidate.lights.reserve(d.actors.size());
        candidate.sceneMaterials.reserve(d.materials.size());
        std::unordered_map<std::uint64_t,MaterialHandle> materials;
        std::unordered_map<std::uint64_t,ActorId> actors;
        // CPU FBX results are kept only for this load, including embedded image data.
        std::unordered_map<std::string,AssetImport::ImportedModelData> imports;
        auto import=[&](const std::filesystem::path& file)->const AssetImport::ImportedModelData&
        {
            const auto key=file.generic_u8string(); auto found=imports.find(key);
            if (found!=imports.end()) return found->second;
            auto result=AssetImport::FbxModelImporter::Import(file);
            Require(static_cast<bool>(result),"Cannot import '"+file.u8string()+"': "+result.error);
            // Import warnings can represent missing textures/unsupported content. Imported geometry
            // is usable, but scene texture references below must all resolve independently.
            return imports.emplace(key,std::move(result.model)).first->second;
        };
        auto texture=[&](const SceneTextureRecord& record,MaterialTextureSlot slot)
        {
            const auto file=ResolveSceneAssetPath(*record.source,roots);
            AssetImport::ImageData image;
            if (record.embeddedId!=0)
            {
                const auto& data=import(file);
                const auto* embedded=AssetImport::FindEmbeddedTexture(data,record.embeddedId);
                Require(embedded!=nullptr,"Embedded FBX texture identity is missing: "+file.u8string());
                image=embedded->image;
            }
            else
            {
                std::string diagnostic;
                const bool loaded=AssetImport::ImageLoader::LoadFromFile(file,image,diagnostic);
                Require(loaded,"Cannot load texture '"+file.u8string()+"': "+diagnostic);
            }
            auto result=Texture2D::CreateRGBA8(renderer.Device(),image.width,image.height,image.pixels,GetRequiredMaterialTextureColorSpace(slot));
            Require(static_cast<bool>(result),"GPU texture creation failed: "+file.u8string());
            result->SetSource(file,record.embeddedId); return result;
        };
        // Parents first, independent of material-table order.
        for (int instance=0;instance<=1;++instance)
            for (const auto& m:d.materials)
            {
                if ((m.parent!=0)!=(instance!=0)) continue;
                MaterialHandle handle;
                if (!m.parent)
                {
                    Material material; material.SetName(m.name); material.GetProperties()=m.properties; material.SetBlendMode(m.blend);
                    for (size_t i=0;i<MaterialTextureSlotCount;++i) if (m.textures[i].source)
                    {
                        const auto slot=static_cast<MaterialTextureSlot>(i);
                        Require(material.SetTexture(slot,texture(m.textures[i],slot),m.textures[i].source->path),"Cannot bind base texture.");
                    }
                    handle=renderer.CreateMaterial(std::move(material));
                }
                else handle=renderer.CreateMaterialInstance(materials.at(m.parent),m.name);
                Require(handle.IsValid(),"GPU material creation failed.");
                candidate.sceneMaterials.push_back(handle); materials.emplace(m.id,handle);
                if (m.parent)
                {
                    for (size_t i=0;i<MaterialParameterCount;++i)
                        Require(renderer.UpdateMaterialOverride(handle,static_cast<MaterialParameter>(i),m.properties,m.overrides.test(i)),"Cannot restore material parameter override.");
                    for (size_t i=0;i<MaterialTextureSlotCount;++i)
                    {
                        const auto slot=static_cast<MaterialTextureSlot>(i); const auto& t=m.textures[i];
                        if (t.mode==TextureOverrideMode::Disabled) Require(renderer.ClearMaterialTexture(handle,slot),"Cannot disable instance texture.");
                        if (t.mode==TextureOverrideMode::Replace)
                            Require(renderer.UpdateMaterialTexture(handle,slot,texture(t,slot),t.source->path),"Cannot replace instance texture.");
                    }
                }
            }
        for (const auto& a:d.actors)
        {
            ActorKind kind=ActorKind::Empty; std::uint64_t component=0;
            if (a.model)
            {
                kind=ActorKind::Model;
                candidate.models.emplace_back(); auto& model=candidate.models.back(); const auto& record=*a.model;
                model.name=a.name; model.id={candidate.nextModelId++}; component=model.id.value;
                model.sourceCubeEdge=record.cubeEdge; model.sourceMeshNames=record.meshNames;
                model.meshes.reserve(record.meshNames.size()); model.sections.reserve(record.sections.size());
                model.materials.reserve(record.sections.size()*2);
                if (record.source)
                {
                    model.sourcePath=ResolveSceneAssetPath(*record.source,roots); const auto& data=import(model.sourcePath);
                    Require(data.meshes.size()==record.meshNames.size(),"FBX mesh count changed: "+model.sourcePath.u8string());
                    model.importedSizeMeters=data.boundsMax-data.boundsMin; model.sourceUnitMeters=data.sourceUnitMeters;
                    for (size_t i=0;i<data.meshes.size();++i)
                    {
                        Require(data.meshes[i].name==record.meshNames[i],"FBX mesh order/name changed: "+model.sourcePath.u8string());
                        const auto h=renderer.CreateMesh(data.meshes[i].vertices,data.meshes[i].indices);
                        Require(h.IsValid(),"GPU mesh creation failed."); model.meshes.push_back(h);
                    }
                }
                else
                {
                    Require(record.meshNames.size()==1,"Built-in cube requires one source mesh.");
                    auto cube=CreateCubeGeometry(record.cubeEdge); const auto h=renderer.CreateMesh(cube.vertices,cube.indices);
                    Require(h.IsValid(),"GPU cube creation failed."); model.meshes.push_back(h);
                }
                for (const auto& s:record.sections)
                {
                    auto track=[&](MaterialHandle handle) {
                        if (std::none_of(model.materials.begin(),model.materials.end(),[&](auto h){return h.id==handle.id;}))
                            model.materials.push_back(handle);
                    };
                    track(materials.at(s.material));
                    const auto sourceMaterial=std::find_if(d.materials.begin(),d.materials.end(),[&](const auto& m){return m.id==s.material;});
                    if (sourceMaterial->parent) track(materials.at(sourceMaterial->parent));
                    SceneModelSection section; section.name=s.name; section.localBounds=s.bounds;
                    section.localTransform=s.local; section.material=materials.at(s.material);
                    section.primitiveId=renderer.AddPrimitive(model.meshes.at(s.mesh),section.material,s.local,s.bounds,s.castsShadow);
                    Require(section.primitiveId!=InvalidPrimitiveId,"GPU primitive registration failed.");
                    model.sections.push_back(std::move(section));
                }
                if (a.id==d.defaultModel) candidate.defaultModelId=model.id;
            }
            if (a.light)
            {
                kind=ActorKind::Light;
                SceneLight light; light.name=a.name; light.proxy=*a.light;
                light.proxy.id=renderer.AddLight(light.proxy); Require(light.IsValid(),"GPU light registration failed.");
                candidate.lights.push_back(std::move(light)); component=candidate.lights.back().proxy.id;
                if (a.id==d.mainLight) candidate.mainLightId=static_cast<LightId>(component);
            }
            if (a.camera)
            {
                kind=ActorKind::Camera; candidate.camera=*a.camera;
                candidate.camera.id={1}; candidate.camera.name=a.name; component=1;
            }
            const auto id=candidate.world.Create(a.name,a.local,kind,component);
            Require(id.IsValid(),"Cannot restore actor transform."); actors.emplace(a.id,id);
            candidate.world.SetVisible(id,a.visible);
            if (a.model) candidate.models.back().actor=id;
            if (a.light) candidate.lights.back().actor=id;
            if (a.camera) candidate.camera.actor=id;
        }
        for (const auto index:SceneActorLoadOrder(d))
        {
            const auto& a=d.actors[index];
            if (a.parent&&!candidate.world.Reparent(actors.at(a.id),actors.at(a.parent),false,error)) throw std::runtime_error(error);
        }
        Require(RegisterSceneActors(candidate),"Cannot register loaded scene.");
        // Force transform/visibility submission even when an identity value matches the default.
        for (auto& m:candidate.models)
        {
            const auto* a=candidate.world.Find(m.actor); m.transform=a->world;
            m.visible=candidate.world.IsVisible(m.actor); ApplySceneModelTransform(m,renderer);
            for (const auto& s:m.sections) renderer.UpdatePrimitiveVisibility(s.primitiveId,m.visible);
        }
        for (auto& l:candidate.lights)
        {
            l.transform=candidate.world.Find(l.actor)->world; l.proxy.visible=candidate.world.IsVisible(l.actor);
            ApplySceneLightTransform(l,renderer);
        }
        SynchronizeSceneActors(candidate,renderer);
        candidate.reflectionGroundEnabled=d.reflectionGroundEnabled; candidate.groundY=d.groundY; candidate.modelDiameter=d.modelDiameter;
        candidate.sceneRadius=d.sceneRadius; candidate.defaultBoundsMin=d.defaultBoundsMin; candidate.defaultBoundsMax=d.defaultBoundsMax; candidate.objCenter=d.objCenter;
        if (!renderer.LoadEnvironment(candidate.environmentSource,error))
            throw std::runtime_error("Environment rebuild failed: " + error);
        // No operation after this point allocates: commit, then release exactly the old resource set.
        active=std::move(candidate); candidate=SceneState{};
        ReleaseSnapshot(old,renderer); error.clear(); return true;
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}
