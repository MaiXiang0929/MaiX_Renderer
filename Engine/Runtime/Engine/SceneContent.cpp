// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/SceneContent.h"
#include "Engine/Runtime/Engine/SceneSubtree.h"
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

namespace { void Require(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); } }
PreparedSceneContent::~PreparedSceneContent()
{
    if (!m_Renderer) return;
    for (const auto& model:scene.models) for (const auto mesh:model.meshes) m_Renderer->DestroyMesh(mesh);
    for (int instances=1;instances>=0;--instances)
        for (const auto material:scene.sceneMaterials)
            if (m_Renderer->IsMaterialInstance(material)==(instances!=0)) m_Renderer->DestroyMaterial(material);
}
bool PrepareSceneContent(const SceneDocument& d,Renderer& renderer,const SceneAssetRoots& roots,
    const SceneContentOptions& options,std::unique_ptr<PreparedSceneContent>& output,std::string& error)
{
    if (output) { error="Prepared content output must be empty."; return false; }
    if (!ValidateSceneDocument(d,error)) return false;
    try {
        auto prepared=std::make_unique<PreparedSceneContent>(renderer);
        auto& candidate=prepared->scene;
        auto& materials=prepared->bindings.materials;
        auto& actors=prepared->bindings.actors;
        const auto& borrowed=options.borrowedMaterials;
        candidate.nextModelId=options.firstModelId;
        candidate.postProcess=d.postProcess; candidate.shadowSettings=d.shadowSettings; candidate.environment=d.environment;
        if (d.environmentSource) candidate.environmentSource=ResolveSceneAssetPath(*d.environmentSource,roots);
        if (!candidate.world.SetContentRegion(options.region,error)) return false;
        if (!options.region.empty()) {
            Require(options.actorGuids.size()==d.actors.size(),"Missing content GUID mappings.");
            for (const auto& a:d.actors) Require(IsContentGuid(options.actorGuids.at(a.id)),"Invalid actor content GUID.");
        }
        for (const auto& entry:borrowed) {
            Renderer::MaterialSnapshot snapshot;
            Require(renderer.GetMaterialSnapshot(entry.second,snapshot),"Shared material is unavailable.");
        }
        candidate.models.reserve(d.actors.size()); candidate.lights.reserve(d.actors.size());
        candidate.sceneMaterials.reserve(d.materials.size());
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
            const auto source=ResolveSceneTextureSource(record,roots);
            AssetImport::ImageData image;
            std::string diagnostic;
            const auto load=[&](const AssetImport::TextureSourceImage& input,AssetImport::ImageData& output,std::string& loadError)
            {
                if (input.embeddedId==0) return AssetImport::ImageLoader::LoadFromFile(input.path,output,loadError);
                const auto& data=import(input.path);
                // Stable identity resolves the raw embedded original, never a prepared normal/ORM.
                const auto* embedded=AssetImport::FindEmbeddedTexture(data,input.embeddedId);
                if (!embedded) { loadError="Embedded FBX texture identity is missing: "+input.path.u8string(); return false; }
                output=embedded->image; return true;
            };
            const bool rebuilt=AssetImport::BuildTextureSourceImage(source,load,image,diagnostic);
            Require(rebuilt,"Cannot reconstruct texture: "+diagnostic);
            auto result=Texture2D::CreateRGBA8(renderer.Device(),image.width,image.height,image.pixels,GetRequiredMaterialTextureColorSpace(slot));
            Require(static_cast<bool>(result),"GPU texture creation failed during scene reconstruction.");
            result->SetReconstructionSource(source); return result;
        };
        // Parents first, independent of material-table order.
        for (int instance=0;instance<=1;++instance)
            for (const auto& m:d.materials)
            {
                if ((m.parent!=0)!=(instance!=0)) continue;
                MaterialHandle handle;
                if (borrowed.count(m.id)) handle=borrowed.at(m.id);
                else if (!m.parent)
                {
                    Material material; material.SetName(m.name); material.GetProperties()=m.properties; material.SetBlendMode(m.blend);
                    for (size_t i=0;i<MaterialTextureSlotCount;++i) if (m.textures[i].source||m.textures[i].recipe)
                    {
                        const auto slot=static_cast<MaterialTextureSlot>(i);
                        Require(material.SetTexture(slot,texture(m.textures[i],slot),m.textures[i].source?m.textures[i].source->path:"ORM"),"Cannot bind base texture.");
                    }
                    handle=renderer.CreateMaterial(std::move(material));
                }
                else handle=renderer.CreateMaterialInstance(materials.at(m.parent),m.name);
                Require(handle.IsValid(),"GPU material creation failed.");
                if (!borrowed.count(m.id)) candidate.sceneMaterials.push_back(handle); materials.emplace(m.id,handle);
                if (m.parent && !borrowed.count(m.id))
                {
                    for (size_t i=0;i<MaterialParameterCount;++i)
                        Require(renderer.UpdateMaterialOverride(handle,static_cast<MaterialParameter>(i),m.properties,m.overrides.test(i)),"Cannot restore material parameter override.");
                    for (size_t i=0;i<MaterialTextureSlotCount;++i)
                    {
                        const auto slot=static_cast<MaterialTextureSlot>(i); const auto& t=m.textures[i];
                        if (t.mode==TextureOverrideMode::Disabled) Require(renderer.ClearMaterialTexture(handle,slot),"Cannot disable instance texture.");
                        if (t.mode==TextureOverrideMode::Replace)
                            Require(renderer.UpdateMaterialTexture(handle,slot,texture(t,slot),t.source?t.source->path:"ORM"),"Cannot replace instance texture.");
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
                    section.primitiveId=renderer.StagePrimitive(model.meshes.at(s.mesh),section.material,s.local,s.bounds,s.castsShadow,prepared->primitives);
                    Require(section.primitiveId!=InvalidPrimitiveId,"GPU primitive registration failed.");
                    model.sections.push_back(std::move(section));
                }
                if (a.id==d.defaultModel) candidate.defaultModelId=model.id;
            }
            if (a.light)
            {
                kind=ActorKind::Light;
                SceneLight light; light.name=a.name; light.proxy=*a.light;
                light.proxy.id=renderer.StageLight(light.proxy,prepared->lights); Require(light.IsValid(),"GPU light registration failed.");
                candidate.lights.push_back(std::move(light)); component=candidate.lights.back().proxy.id;
                if (a.id==d.mainLight) candidate.mainLightId=static_cast<LightId>(component);
            }
            if (a.camera)
            {
                kind=ActorKind::Camera; candidate.camera=*a.camera;
                candidate.camera.id={1}; candidate.camera.name=a.name; component=1;
            }
            const auto id=candidate.world.Create(a.name,a.local,kind,component,options.region.empty()?std::string{}:options.actorGuids.at(a.id));
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
            m.visible=candidate.world.IsVisible(m.actor);
            for (const auto& s:m.sections) {
                auto& proxy=*std::find_if(prepared->primitives.begin(),prepared->primitives.end(),[&](const auto& p) { return p.id==s.primitiveId; });
                proxy.localToWorld=m.transform.ToMatrix()*s.localTransform; proxy.visible=m.visible;
            }
        }
        for (auto& l:candidate.lights)
        {
            l.transform=candidate.world.Find(l.actor)->world; l.proxy.visible=candidate.world.IsVisible(l.actor);
            UpdateSceneLightProxy(l);
            *std::find_if(prepared->lights.begin(),prepared->lights.end(),[&](const auto& light) { return light.id==l.proxy.id; })=l.proxy;
        }

        candidate.reflectionGroundEnabled=d.reflectionGroundEnabled; candidate.groundY=d.groundY; candidate.modelDiameter=d.modelDiameter;
        candidate.sceneRadius=d.sceneRadius; candidate.defaultBoundsMin=d.defaultBoundsMin; candidate.defaultBoundsMax=d.defaultBoundsMax; candidate.objCenter=d.objCenter;

        output=std::move(prepared); error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}
