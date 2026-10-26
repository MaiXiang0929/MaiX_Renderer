// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/SceneContentPreparation.h"
#include "Engine/Asset/Importer/FbxModelImporter.h"
#include "Engine/Asset/Importer/ImageLoader.h"
#include "Engine/Asset/Geometry/CubeGeometry.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>

namespace {
using Clock=std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
void Require(bool ok,const std::string& error) { if(!ok) throw std::runtime_error(error); }
std::size_t MeshBytes(const AssetImport::ImportedMeshData& mesh)
{ return mesh.vertices.size()*sizeof(Vertex)+mesh.indices.size()*sizeof(std::uint32_t); }
}
bool PrepareSceneCpuContent(const SceneDocument& document,const SceneAssetRoots& roots,const ScenePreparationLimits& limits,
    const std::shared_ptr<std::atomic_bool>& cancel,std::unique_ptr<SceneCpuContent>& output,std::string& error)
{
    if(output) { error="CPU content output must be empty."; return false; }
    const auto start=Clock::now();
    try {
        Require(ValidateSceneDocument(document,error),error);
        auto data=std::make_unique<SceneCpuContent>(); data->document=document; data->roots=roots;
        std::string encoded; Require(EncodeSceneDocument(document,encoded,error),error);
        data->cpuBytes=encoded.size();
        auto check=[&] {
            Require(!cancel||!cancel->load(),"Content preparation cancelled.");
            Require(data->cpuBytes<=limits.cpuBytes,"Region CPU candidate exceeds preparation budget.");
        };
        auto sourceCheck=[&](const std::filesystem::path& source) {
            check(); Require(std::filesystem::file_size(source)<=limits.sourceBytes,"Source file exceeds preparation limit: "+source.u8string());
        };
        auto account=[&](std::size_t bytes) {
            Require(bytes<=limits.resourceBytes,"Single mesh/texture exceeds upload safety limit.");
            Require(bytes<=limits.cpuBytes&&data->cpuBytes<=limits.cpuBytes-bytes,"Region CPU candidate exceeds preparation budget.");
            data->cpuBytes+=bytes; data->gpuBytes+=bytes; check();
        };
        std::map<std::string,AssetImport::ImportedModelData> imports;
        auto import=[&](const std::filesystem::path& source)->const AssetImport::ImportedModelData& {
            sourceCheck(source); const auto key=source.generic_u8string(); auto found=imports.find(key);
            if(found!=imports.end()) return found->second;
            auto result=AssetImport::FbxModelImporter::Import(source);
            Require(static_cast<bool>(result),"Cannot import '"+source.u8string()+"': "+result.error); check();
            return imports.emplace(key,std::move(result.model)).first->second;
        };
        for(const auto& material:document.materials) for(std::size_t slot=0;slot<MaterialTextureSlotCount;++slot) {
            const auto& record=material.textures[slot];
            if(!(record.source||record.recipe)) continue;
            check(); SceneCpuTexture texture; texture.source=ResolveSceneTextureSource(record,roots);
            auto load=[&](const AssetImport::TextureSourceImage& input,AssetImport::ImageData& image,std::string& diagnostic) {
                sourceCheck(input.path);
                if(input.embeddedId==0) return AssetImport::ImageLoader::LoadFromFile(input.path,image,diagnostic);
                const auto* embedded=AssetImport::FindEmbeddedTexture(import(input.path),input.embeddedId);
                if(!embedded) { diagnostic="Embedded FBX texture identity missing."; return false; }
                image=embedded->image; return true;
            };
            const bool reconstructed=AssetImport::BuildTextureSourceImage(texture.source,load,texture.image,error);
            Require(reconstructed,"Cannot reconstruct texture: "+error);
            const auto bytes=texture.image.pixels.size();
            // RGBA8 mip pyramid allocation estimate (driver overhead excluded).
            account(bytes);
            std::size_t mipBytes=0;
            for(unsigned w=texture.image.width,h=texture.image.height;;w=std::max(1u,w/2),h=std::max(1u,h/2)) {
                mipBytes+=static_cast<std::size_t>(w)*h*4; if(w==1&&h==1) break;
            }
            data->gpuBytes+=mipBytes-bytes;
            data->textures.emplace(std::make_pair(material.id,slot),std::move(texture));
        }
        for(const auto& actor:document.actors) if(actor.model) {
            check(); const auto& record=*actor.model; SceneCpuModel model;
            if(record.source) {
                model.source=ResolveSceneAssetPath(*record.source,roots); const auto& imported=import(model.source);
                Require(imported.meshes.size()==record.meshNames.size(),"FBX mesh count changed: "+model.source.u8string());
                model.size=imported.boundsMax-imported.boundsMin; model.sourceUnitMeters=imported.sourceUnitMeters;
                for(std::size_t i=0;i<imported.meshes.size();++i) {
                    Require(imported.meshes[i].name==record.meshNames[i],"FBX mesh name/order changed.");
                    account(MeshBytes(imported.meshes[i])); model.meshes.push_back(imported.meshes[i]);
                }
            } else {
                Require(record.meshNames.size()==1,"Built-in cube requires one source mesh.");
                auto cube=CreateCubeGeometry(record.cubeEdge); AssetImport::ImportedMeshData mesh;
                mesh.vertices=std::move(cube.vertices); mesh.indices=std::move(cube.indices);
                account(MeshBytes(mesh)); model.meshes.push_back(std::move(mesh));
            }
            data->models.emplace(actor.id,std::move(model));
        }
        check(); data->preparationMs=Milliseconds(start); output=std::move(data); error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}

struct SceneContentUpload::Impl
{
    struct Operation { std::size_t bytes; std::function<void()> execute; };
    Renderer& renderer;
    std::unique_ptr<SceneCpuContent> cpu;
    SceneContentOptions options;
    std::unique_ptr<PreparedSceneContent> result;
    std::map<std::pair<std::uint64_t,std::size_t>,std::shared_ptr<Texture2D>> textures;
    std::map<std::uint64_t,std::vector<MeshHandle>> meshes;
    std::vector<Operation> operations;
    std::size_t cursor=0;
    double lastMs=0;
    Impl(std::unique_ptr<SceneCpuContent> data,Renderer& r,const SceneContentOptions& o)
        :renderer(r),cpu(std::move(data)),options(o),result(std::make_unique<PreparedSceneContent>(r)) {}
    void AddActor(const SceneActorRecord& a);
    void Finish();
};
SceneContentUpload::SceneContentUpload()=default;
SceneContentUpload::~SceneContentUpload()=default;
bool SceneContentUpload::Begin(std::unique_ptr<SceneCpuContent> cpu,Renderer& renderer,const SceneContentOptions& options,std::string& error)
{
    try {
        Require(!m_Impl&&static_cast<bool>(cpu),"Upload already begun or missing CPU content.");
        auto state=std::make_unique<Impl>(std::move(cpu),renderer,options); auto* p=state.get();
        auto& scene=p->result->scene; const auto& d=p->cpu->document;
        scene.nextModelId=options.firstModelId; scene.postProcess=d.postProcess; scene.shadowSettings=d.shadowSettings; scene.environment=d.environment;
        if(d.environmentSource) scene.environmentSource=ResolveSceneAssetPath(*d.environmentSource,p->cpu->roots);
        Require(scene.world.SetContentRegion(options.region,error),error);
        for(const auto& a:d.actors) if(!options.region.empty()) Require(IsContentGuid(options.actorGuids.at(a.id)),"Missing actor content GUID.");
        for(const auto& entry:options.borrowedMaterials) {
            Renderer::MaterialSnapshot snapshot; Require(renderer.GetMaterialSnapshot(entry.second,snapshot),"Shared material unavailable.");
        }
        // Candidate cleanup owns meshes immediately, including those not yet assembled into actors.
        std::size_t meshCount=0; for(const auto& entry:p->cpu->models) meshCount+=entry.second.meshes.size();
        p->result->ownedMeshes.reserve(meshCount); scene.sceneMaterials.reserve(d.materials.size());
        scene.models.reserve(p->cpu->models.size()); scene.lights.reserve(d.actors.size());
        for(const auto& entry:p->cpu->textures) {
            const auto key=entry.first;
            p->operations.push_back({entry.second.image.pixels.size(),[p,key] {
                const auto& t=p->cpu->textures.at(key);
                auto texture=Texture2D::CreateRGBA8(p->renderer.Device(),t.image.width,t.image.height,t.image.pixels,
                    GetRequiredMaterialTextureColorSpace(static_cast<MaterialTextureSlot>(key.second)));
                Require(static_cast<bool>(texture),"GPU texture creation failed."); texture->SetReconstructionSource(t.source);
                p->textures.emplace(key,std::move(texture));
            }});
        }
        for(int instance=0;instance<=1;++instance) for(std::size_t i=0;i<d.materials.size();++i) {
            if((d.materials[i].parent!=0)!=(instance!=0)) continue;
            p->operations.push_back({0,[p,i] {
                const auto& m=p->cpu->document.materials[i]; auto& materials=p->result->bindings.materials;
                auto borrowed=p->options.borrowedMaterials.find(m.id); MaterialHandle handle;
                if(borrowed!=p->options.borrowedMaterials.end()) handle=borrowed->second;
                else if(!m.parent) {
                    Material material; material.SetName(m.name); material.GetProperties()=m.properties; material.SetBlendMode(m.blend);
                    for(std::size_t slot=0;slot<MaterialTextureSlotCount;++slot) {
                        auto t=p->textures.find({m.id,slot}); if(t==p->textures.end()) continue;
                        Require(material.SetTexture(static_cast<MaterialTextureSlot>(slot),t->second,m.textures[slot].source?m.textures[slot].source->path:"ORM"),"Cannot bind texture.");
                    }
                    handle=p->renderer.CreateMaterial(std::move(material));
                } else handle=p->renderer.CreateMaterialInstance(materials.at(m.parent),m.name);
                Require(handle.IsValid(),"Cannot create GPU material.");
                if(borrowed==p->options.borrowedMaterials.end()) p->result->scene.sceneMaterials.push_back(handle);
                materials.emplace(m.id,handle);
                if(m.parent&&borrowed==p->options.borrowedMaterials.end()) {
                    for(std::size_t parameter=0;parameter<MaterialParameterCount;++parameter)
                        Require(p->renderer.UpdateMaterialOverride(handle,static_cast<MaterialParameter>(parameter),m.properties,m.overrides.test(parameter)),"Cannot restore override.");
                    for(std::size_t slot=0;slot<MaterialTextureSlotCount;++slot) {
                        const auto& t=m.textures[slot]; const auto s=static_cast<MaterialTextureSlot>(slot);
                        if(t.mode==TextureOverrideMode::Disabled) Require(p->renderer.ClearMaterialTexture(handle,s),"Cannot disable texture.");
                        if(t.mode==TextureOverrideMode::Replace) Require(p->renderer.UpdateMaterialTexture(handle,s,p->textures.at({m.id,slot}),t.source?t.source->path:"ORM"),"Cannot restore texture.");
                    }
                }
            }});
        }
        for(const auto& entry:p->cpu->models) {
            const auto id=entry.first; p->meshes[id].reserve(entry.second.meshes.size());
            for(std::size_t i=0;i<entry.second.meshes.size();++i) p->operations.push_back({MeshBytes(entry.second.meshes[i]),[p,id,i] {
                const auto& mesh=p->cpu->models.at(id).meshes[i];
                auto handle=p->renderer.CreateMesh(mesh.vertices,mesh.indices); Require(handle.IsValid(),"GPU mesh creation failed.");
                p->result->ownedMeshes.push_back(handle); p->meshes.at(id).push_back(handle);
            }});
        }
        for(std::size_t i=0;i<d.actors.size();++i) p->operations.push_back({0,[p,i] { p->AddActor(p->cpu->document.actors[i]); }});
        p->operations.push_back({0,[p] { p->Finish(); }});
        m_Impl=std::move(state); error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}
void SceneContentUpload::Impl::AddActor(const SceneActorRecord& a)
{
    auto& candidate=result->scene; auto& materials=result->bindings.materials;
    ActorKind kind=ActorKind::Empty; std::uint64_t component=0;
    if(a.model) {
        kind=ActorKind::Model; candidate.models.emplace_back(); auto& model=candidate.models.back(); const auto& record=*a.model;
        Require(candidate.nextModelId!=std::numeric_limits<std::uint64_t>::max(),"Model identity exhausted.");
        model.name=a.name; model.id={candidate.nextModelId++}; component=model.id.value;
        model.sourceCubeEdge=record.cubeEdge; model.sourceMeshNames=record.meshNames;
        const auto& data=cpu->models.at(a.id); model.sourcePath=data.source; model.importedSizeMeters=data.size; model.sourceUnitMeters=data.sourceUnitMeters;
        model.meshes=meshes.at(a.id);
        for(const auto& s:record.sections) {
            auto track=[&](MaterialHandle h) { if(std::none_of(model.materials.begin(),model.materials.end(),[&](auto old) { return old.id==h.id; })) model.materials.push_back(h); };
            track(materials.at(s.material));
            const auto source=std::find_if(cpu->document.materials.begin(),cpu->document.materials.end(),[&](const auto& m) { return m.id==s.material; });
            if(source->parent) track(materials.at(source->parent));
            SceneModelSection section; section.name=s.name; section.localBounds=s.bounds; section.localTransform=s.local;
            section.material=materials.at(s.material);
            section.primitiveId=renderer.StagePrimitive(model.meshes.at(s.mesh),section.material,s.local,s.bounds,s.castsShadow,result->primitives);
            Require(section.primitiveId!=InvalidPrimitiveId,"Cannot stage primitive."); model.sections.push_back(std::move(section));
        }
        if(a.id==cpu->document.defaultModel) candidate.defaultModelId=model.id;
    }
    if(a.light) {
        kind=ActorKind::Light; SceneLight light; light.name=a.name; light.proxy=*a.light;
        light.proxy.id=renderer.StageLight(light.proxy,result->lights); Require(light.IsValid(),"Cannot stage light.");
        component=light.proxy.id; candidate.lights.push_back(std::move(light));
        if(a.id==cpu->document.mainLight) candidate.mainLightId=static_cast<LightId>(component);
    }
    if(a.camera) { kind=ActorKind::Camera; candidate.camera=*a.camera; candidate.camera.id={1}; candidate.camera.name=a.name; component=1; }
    const auto id=candidate.world.Create(a.name,a.local,kind,component,options.region.empty()?std::string{}:options.actorGuids.at(a.id));
    Require(id.IsValid(),"Cannot restore actor."); result->bindings.actors.emplace(a.id,id); candidate.world.SetVisible(id,a.visible);
    if(a.model) candidate.models.back().actor=id;
    if(a.light) candidate.lights.back().actor=id;
    if(a.camera) candidate.camera.actor=id;
}
void SceneContentUpload::Impl::Finish()
{
    auto& candidate=result->scene; const auto& d=cpu->document; std::string error;
    for(const auto index:SceneActorLoadOrder(d)) {
        const auto& a=d.actors[index];
        if(a.parent) Require(candidate.world.Reparent(result->bindings.actors.at(a.id),result->bindings.actors.at(a.parent),false,error),error);
    }
    Require(RegisterSceneActors(candidate),"Cannot register candidate actors.");
    std::unordered_map<PrimitiveId,PrimitiveSceneProxy*> proxies;
    for(auto& proxy:result->primitives) proxies.emplace(proxy.id,&proxy);
    for(auto& model:candidate.models) {
        model.transform=candidate.world.Find(model.actor)->world; model.visible=candidate.world.IsVisible(model.actor);
        for(const auto& section:model.sections) { auto& proxy=*proxies.at(section.primitiveId); proxy.localToWorld=model.transform.ToMatrix()*section.localTransform; proxy.visible=model.visible; }
    }
    for(std::size_t i=0;i<candidate.lights.size();++i) {
        auto& light=candidate.lights[i]; light.transform=candidate.world.Find(light.actor)->world;
        light.proxy.visible=candidate.world.IsVisible(light.actor); UpdateSceneLightProxy(light); result->lights[i]=light.proxy;
    }
    candidate.reflectionGroundEnabled=d.reflectionGroundEnabled; candidate.groundY=d.groundY; candidate.modelDiameter=d.modelDiameter;
    candidate.sceneRadius=d.sceneRadius; candidate.defaultBoundsMin=d.defaultBoundsMin; candidate.defaultBoundsMax=d.defaultBoundsMax; candidate.objCenter=d.objCenter;
}
bool SceneContentUpload::Step(std::size_t operations,std::size_t bytes,double milliseconds,std::string& error)
{
    const auto start=Clock::now();
    try {
        Require(static_cast<bool>(m_Impl),"No upload in progress."); std::size_t done=0,used=0;
        while(m_Impl->cursor<m_Impl->operations.size()&&done<operations) {
            const auto& operation=m_Impl->operations[m_Impl->cursor];
            if(done&&(Milliseconds(start)>=milliseconds||operation.bytes>bytes-used)) break;
            operation.execute(); ++m_Impl->cursor; ++done; used+=operation.bytes;
            if(used>=bytes) break;
        }
        m_Impl->lastMs=Milliseconds(start); error.clear(); return true;
    } catch(const std::exception& e) { if(m_Impl) m_Impl->lastMs=Milliseconds(start); error=e.what(); return false; }
}
bool SceneContentUpload::Complete() const { return m_Impl&&m_Impl->cursor==m_Impl->operations.size(); }
std::size_t SceneContentUpload::CompletedOperations() const { return m_Impl?m_Impl->cursor:0; }
std::size_t SceneContentUpload::TotalOperations() const { return m_Impl?m_Impl->operations.size():0; }
double SceneContentUpload::LastStepMs() const { return m_Impl?m_Impl->lastMs:0; }
std::unique_ptr<PreparedSceneContent> SceneContentUpload::Take() { return Complete()?std::move(m_Impl->result):nullptr; }
SceneContentRetirement SceneContentUpload::Abandon()
{
    SceneContentRetirement retired;
    if(!m_Impl) return retired;
    if(m_Impl->result) {
        retired.meshes=std::move(m_Impl->result->ownedMeshes);
        retired.materials=std::move(m_Impl->result->scene.sceneMaterials);
        std::stable_sort(retired.materials.begin(),retired.materials.end(),[&](auto a,auto b) {
            return m_Impl->renderer.IsMaterialInstance(a)>m_Impl->renderer.IsMaterialInstance(b);
        });
        m_Impl->result->Release();
    }
    retired.textures.reserve(m_Impl->textures.size());
    for(auto& entry:m_Impl->textures) retired.textures.push_back(std::move(entry.second));
    m_Impl.reset(); return retired;
}
