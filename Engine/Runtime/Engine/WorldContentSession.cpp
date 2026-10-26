// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/WorldContentSession.h"
#include "Engine/Runtime/Engine/SceneContent.h"
#include "Engine/Runtime/Engine/ScenePersistence.h"
#include "Engine/Runtime/Engine/SceneSubtree.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <algorithm>
#include <unordered_set>
#include <map>
#include <atomic>
#include "Engine/Runtime/Scene/ContentComparison.h"
#include <stdexcept>

namespace
{
void Check(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); }
template<class F> bool Guard(std::string& error,F&& action)
{ try { action(); error.clear(); return true; } catch(const std::exception& e) { error=e.what(); return false; } }
template<class T> void Add(std::vector<T>& list,T value)
{ if (std::none_of(list.begin(),list.end(),[&](T old) { return old.id==value.id; })) list.push_back(value); }
template<class T> bool Has(const std::vector<T>& list,T value)
{ return std::any_of(list.begin(),list.end(),[&](T old) { return old.id==value.id; }); }
ContentDocument Canonical(ContentDocument value)
{
    std::map<std::uint64_t,std::uint64_t> actors,materials;
    auto keys=[](auto& values,auto& mapping) {
        std::sort(values.begin(),values.end(),[](const auto& a,const auto& b) { return a.guid<b.guid; });
        for(std::size_t i=0;i<values.size();++i) { mapping.emplace(values[i].key,i+1); values[i].key=i+1; }
    };
    keys(value.actors,actors); keys(value.materials,materials);
    for(auto& actor:value.scene.actors) {
        actor.id=actors.at(actor.id); if(actor.parent) actor.parent=actors.at(actor.parent);
        if(actor.model) for(auto& section:actor.model->sections) section.material=materials.at(section.material);
    }
    for(auto& material:value.scene.materials) {
        material.id=materials.at(material.id); if(material.parent) material.parent=materials.at(material.parent);
    }
    if(value.scene.defaultModel) value.scene.defaultModel=actors.at(value.scene.defaultModel);
    if(value.scene.mainLight) value.scene.mainLight=actors.at(value.scene.mainLight);
    auto order=[](const auto& a,const auto& b) { return a.id<b.id; };
    // Actor sequence is authored content: it determines stable light and shadow priority.
    std::sort(value.scene.materials.begin(),value.scene.materials.end(),order);
    return value;
}
std::string WorldText(WorldContentDocument value)
{
    value.persistent=Canonical(std::move(value.persistent)); std::string text,error;
    Check(EncodeWorldContentDocument(value,text,error),error); return text;
}
void AddDescriptors(const ContentDocument& content,const std::string& region,std::vector<ContentActorDescriptor>& result)
{
    std::map<std::uint64_t,std::string> guids;
    std::unordered_set<std::string> existing;
    for(const auto& descriptor:result) existing.insert(descriptor.guid);
    for(const auto& key:content.actors) guids.emplace(key.key,key.guid);
    for(const auto& actor:content.scene.actors) {
        const auto guid=guids.at(actor.id); Check(existing.insert(guid).second,"Actor GUID is duplicated across content files.");
        result.push_back({guid,region,actor.name,actor.parent?guids.at(actor.parent):std::string{},
            actor.model?ActorKind::Model:actor.light?ActorKind::Light:actor.camera?ActorKind::Camera:ActorKind::Empty});
    }
}
std::filesystem::path WorldPath(const std::filesystem::path& file,const SceneAssetRoots& roots)
{
    const auto path=std::filesystem::weakly_canonical(file);
    Check(path.is_absolute()&&path.extension()==".maixworld"&&MakeSceneAssetPath(path,roots).root=="project",
        "World files must be inside the current project's Assets and use .maixworld."); return path;
}
const SceneContentMaterial* MaterialInfo(const SceneState& scene,MaterialHandle handle)
{
    const auto found=std::find_if(scene.contentMaterials.begin(),scene.contentMaterials.end(),[&](const auto& entry) { return entry.handle.id==handle.id; });
    return found==scene.contentMaterials.end()?nullptr:&*found;
}
void FillOptions(const ContentDocument& content,const std::string& region,const SceneState& scene,SceneContentOptions& options)
{
    options.region=region; options.firstModelId=scene.nextModelId;
    for(const auto& key:content.actors) options.actorGuids.emplace(key.key,key.guid);
    for(const auto& key:content.materials) if(key.shared) {
        const auto found=std::find_if(scene.contentMaterials.begin(),scene.contentMaterials.end(),[&](const auto& material) {
            return material.guid==key.guid&&material.region==scene.contentWorld; });
        Check(found!=scene.contentMaterials.end(),"Shared world material is unavailable: "+key.guid);
        options.borrowedMaterials.emplace(key.key,found->handle);
    }
}
void TrackPrepared(const ContentDocument& content,const std::string& region,const PreparedSceneContent& prepared,std::vector<SceneContentMaterial>& materials)
{
    for(const auto& key:content.materials) if(!key.shared) materials.push_back({prepared.bindings.materials.at(key.key),key.guid,region});
}
void Release(const Renderer::SceneResourceSnapshot& resources,Renderer& renderer)
{
    for(auto primitive:resources.primitives) renderer.RemovePrimitive(primitive);
    for(auto light:resources.lights) renderer.RemoveLight(light);
    for(auto mesh:resources.meshes) renderer.DestroyMesh(mesh);
    for(int instances=1;instances>=0;--instances) for(auto material:resources.materials)
        if(renderer.IsMaterialInstance(material)==(instances!=0)) renderer.DestroyMaterial(material);
}
void ValidateMaterialKeys(const ContentDocument& content,const std::string& world,std::map<std::string,std::string>& owners,const std::string& region)
{
    for(const auto& key:content.materials) {
        const auto found=owners.find(key.guid);
        if(key.shared) Check(found!=owners.end()&&found->second==world,"Unknown shared world material GUID.");
        else Check(owners.emplace(key.guid,region).second,"Material GUID is duplicated across content files.");
    }
}
}

namespace { std::atomic<std::uint64_t> ContentGeneration{1}; }
WorldContentSession::WorldContentSession():m_Generation(ContentGeneration.fetch_add(1)) {}
void WorldContentSession::Reset() noexcept
{ m_Generation=ContentGeneration.fetch_add(1); m_Document={}; m_File.clear(); m_Regions.clear(); m_Catalog.clear(); m_SavedWorld.clear(); }

bool WorldContentSession::Create(const std::filesystem::path& file,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    return Guard(error,[&] {
        Check(!Ready()&&scene.contentWorld.empty(),"A world is already open. Open a standalone scene before creating another world.");
        SceneDocument completeScene;
        Check(CaptureSceneDocument(scene,renderer,roots,completeScene,error),error);
        WorldContentSession next; next.m_File=WorldPath(file,roots); next.m_Document.id=NewContentGuid();
        SceneContentSelection selection; selection.materials=GetSceneMaterialHandles(scene,renderer);
        for(const auto& actor:scene.world.Actors()) selection.actors.push_back(actor.id);
        SceneContentBindings bindings;
        Check(CaptureSceneContent(scene,renderer,roots,selection,next.m_Document.persistent.scene,bindings,error),error);
        World prepared; Check(scene.world.PrepareContentIdentity(next.Id(),prepared,error),error);
        std::vector<SceneContentMaterial> materials;
        for(const auto& entry:bindings.actors) next.m_Document.persistent.actors.push_back({entry.first,prepared.Find(entry.second)->contentGuid,false});
        for(const auto& entry:bindings.materials) {
            auto guid=NewContentGuid(); next.m_Document.persistent.materials.push_back({entry.first,guid,false});
            materials.push_back({entry.second,std::move(guid),next.Id()});
        }
        AddDescriptors(next.m_Document.persistent,next.Id(),next.m_Catalog);
        next.m_SavedWorld=WorldText(next.m_Document);
        auto identity=next.Id(); auto activeMaterials=selection.materials;
        Check(WriteWorldContentDocument(next.m_File,next.m_Document,error,false),error);
        scene.world=std::move(prepared); scene.contentWorld.swap(identity); scene.contentMaterials.swap(materials);
        scene.sceneMaterials.swap(activeMaterials); *this=std::move(next);
    });
}

bool WorldContentSession::Open(const std::filesystem::path& file,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    return Guard(error,[&] {
        WorldContentSession next; next.m_File=WorldPath(file,roots);
        Check(ReadWorldContentDocument(next.m_File,next.m_Document,error),error);
        AddDescriptors(next.m_Document.persistent,next.Id(),next.m_Catalog);
        std::map<std::string,std::string> materialOwners;
        ValidateMaterialKeys(next.m_Document.persistent,next.Id(),materialOwners,next.Id());
        for(const auto& record:next.m_Document.regions) {
            RegionContentDocument region;
            Check(ReadRegionContentDocument(ResolveSceneAssetPath(record.source,roots),region,error),error);
            Check(region.world==next.Id()&&region.id==record.id,"Region identity does not match the world manifest.");
            AddDescriptors(region.content,region.id,next.m_Catalog);
            ValidateMaterialKeys(region.content,next.Id(),materialOwners,region.id);
            next.m_Regions.push_back({record,false,{},{}});
        }
        next.m_SavedWorld=WorldText(next.m_Document);
        const auto old=renderer.GetSceneResources();
        SceneContentOptions options; SceneState empty;
        FillOptions(next.m_Document.persistent,next.Id(),empty,options);
        std::unique_ptr<PreparedSceneContent> prepared;
        Check(PrepareSceneContent(next.m_Document.persistent.scene,renderer,roots,options,prepared,error),error);
        prepared->scene.contentWorld=next.Id();
        TrackPrepared(next.m_Document.persistent,next.Id(),*prepared,prepared->scene.contentMaterials);
        RenderScene renderScene;
        Check(renderer.PrepareSceneUpdate(prepared->primitives,prepared->lights,old.primitives,old.lights,renderScene,error),error);
        Check(renderer.LoadEnvironment(prepared->scene.environmentSource,error),error);
        renderer.CommitSceneUpdate(std::move(renderScene)); scene=std::move(prepared->scene); prepared->Release();
        *this=std::move(next); Release(old,renderer);
    });
}

bool WorldContentSession::TrackMaterials(SceneState& scene,const Renderer& renderer,const std::vector<MaterialHandle>& handles,
    const std::string& region,std::string& error) const
{
    if(!Ready()) { error.clear(); return true; }
    return Guard(error,[&] {
        Check(scene.contentWorld==Id()&&IsContentGuid(region),"Invalid material content owner.");
        auto tracked=scene.contentMaterials;
        for(const auto handle:handles) {
            Renderer::MaterialSnapshot snapshot; Check(renderer.GetMaterialSnapshot(handle,snapshot),"Cannot track a stale material.");
            auto found=std::find_if(tracked.begin(),tracked.end(),[&](const auto& m) { return m.handle.id==handle.id; });
            if(found!=tracked.end()) { Check(found->region==region||found->region==Id(),"A region cannot own another region's material."); continue; }
            if(snapshot.parent.IsValid()) {
                const auto parent=std::find_if(tracked.begin(),tracked.end(),[&](const auto& m) { return m.handle.id==snapshot.parent.id; });
                Check(parent!=tracked.end()&&(parent->region==region||parent->region==Id()),"Material parent belongs to another region.");
            }
            tracked.push_back({handle,NewContentGuid(),region});
        }
        scene.contentMaterials.swap(tracked);
    });
}
bool WorldContentSession::CanBindMaterial(const SceneState& scene,ActorId actor,MaterialHandle material) const
{
    if(!Ready()) return true;
    const auto* owner=scene.world.Find(actor); const auto* value=MaterialInfo(scene,material);
    return owner&&value&&(value->region==Id()||value->region==owner->contentRegion);
}

bool WorldContentSession::Capture(const std::string& region,SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,
    ContentDocument& result,std::string& error) const
{
    return Guard(error,[&] {
        Check(Ready()&&scene.contentWorld==Id(),"No matching active world content session.");
        SceneContentSelection selection;
        for(const auto& actor:scene.world.Actors()) if(actor.contentRegion==region) selection.actors.push_back(actor.id);
        const auto live=GetSceneMaterialHandles(scene,renderer);
        for(const auto& entry:scene.contentMaterials) if(entry.region==region&&Has(live,entry.handle)) Add(selection.materials,entry.handle);
        auto add=[&](MaterialHandle handle) {
            for(;handle.IsValid();handle=renderer.GetMaterialParent(handle)) {
                const auto* entry=MaterialInfo(scene,handle);
                Check(entry&&(entry->region==region||entry->region==Id()),"Untracked material or cross-region material binding.");
                Add(selection.materials,handle);
                if(region!=Id()&&entry->region==Id()) break; // The world owns this definition and its parent closure.
            }
        };
        for(const auto& model:scene.models) {
            const auto* actor=scene.world.Find(model.actor); if(!actor||actor->contentRegion!=region) continue;
            for(auto handle:model.materials) {
                const auto* identity=MaterialInfo(scene,handle);
                if(identity&&identity->region==region) add(handle);
            }
            for(const auto& section:model.sections) add(section.material);
        }
        // Parents of unbound instances must be captured too.
        for(std::size_t i=0;i<selection.materials.size();++i) add(selection.materials[i]);
        std::sort(selection.materials.begin(),selection.materials.end(),[](auto a,auto b) { return a.id<b.id; });
        for(auto handle:selection.materials) {
            const auto* identity=MaterialInfo(scene,handle);
            if(identity&&identity->region!=region) selection.sharedMaterials.push_back(handle);
        }
        ContentDocument captured; SceneContentBindings bindings;
        Check(CaptureSceneContent(scene,renderer,roots,selection,captured.scene,bindings,error),error);
        for(const auto& entry:bindings.actors) {
            const auto* actor=scene.world.Find(entry.second);
            Check(actor&&IsContentGuid(actor->contentGuid),"Actor content identity is unavailable.");
            captured.actors.push_back({entry.first,actor->contentGuid,false});
        }
        for(const auto& entry:bindings.materials) {
            const auto* identity=MaterialInfo(scene,entry.second); Check(identity!=nullptr,"Material has no content identity.");
            const bool shared=identity->region!=region;
            captured.materials.push_back({entry.first,identity->guid,shared});
            if(shared) {
                auto& record=*std::find_if(captured.scene.materials.begin(),captured.scene.materials.end(),[&](const auto& m) { return m.id==entry.first; });
                record=SceneMaterialRecord{}; record.id=entry.first;
            }
        }
        if(region!=Id()) {
            auto actors=std::move(captured.scene.actors); auto materials=std::move(captured.scene.materials);
            captured.scene=SceneDocument{}; captured.scene.actors=std::move(actors); captured.scene.materials=std::move(materials);
        }
        Check(ValidateContentDocument(captured,region!=Id(),error),error); result=std::move(captured);
    });
}

bool WorldContentSession::RefreshCatalog(const SceneState& scene,std::string& error)
{
    if(!Ready()) { error.clear(); return true; }
    return Guard(error,[&] {
        Check(scene.contentWorld==Id(),"Content directory belongs to another scene.");
        std::vector<ContentActorDescriptor> catalog;
        for(const auto& entry:m_Catalog) {
            const auto region=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==entry.region; });
            if(region!=m_Regions.end()&&!region->loaded) catalog.push_back(entry);
        }
        std::unordered_set<std::string> ids; for(const auto& entry:catalog) ids.insert(entry.guid);
        for(const auto& actor:scene.world.Actors()) {
            Check(IsContentGuid(actor.contentGuid)&&IsContentGuid(actor.contentRegion)&&ids.insert(actor.contentGuid).second,"Invalid or duplicate active content GUID.");
            const auto* parent=scene.world.Find(actor.parent);
            catalog.push_back({actor.contentGuid,actor.contentRegion,actor.name,parent?parent->contentGuid:std::string{},actor.kind});
        }
        m_Catalog.swap(catalog);
    });
}
bool WorldContentSession::SetEditingRegion(const std::string& region,SceneState& scene,std::string& error) const
{
    if(!Ready()||scene.contentWorld!=Id()) { error="No active world."; return false; }
    const auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==region&&r.loaded; });
    if(region!=Id()&&found==m_Regions.end()) { error="Load the region before editing its content."; return false; }
    return scene.world.SetContentRegion(region,error);
}

bool WorldContentSession::SavePersistent(SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    return Guard(error,[&] {
        auto document=m_Document; Check(Capture(Id(),scene,renderer,roots,document.persistent,error),error);
        auto text=WorldText(document); Check(WriteWorldContentDocument(m_File,document,error),error);
        m_Document=std::move(document); m_SavedWorld=std::move(text);
    });
}
bool WorldContentSession::SaveRegion(const std::string& id,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    if(id==Id()) return SavePersistent(scene,renderer,roots,error);
    return Guard(error,[&] {
        auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==id; });
        Check(found!=m_Regions.end()&&found->loaded,"Load the region before saving it.");
        RegionContentDocument document{Id(),id,{}}; Check(Capture(id,scene,renderer,roots,document.content,error),error);
        auto baseline=Canonical(document.content); const auto file=ResolveSceneAssetPath(found->record.source,roots);
        Check(SavePersistent(scene,renderer,roots,error),error);
        Check(WriteRegionContentDocument(file,document,error),error); found->baseline=std::move(baseline);
    });
}
bool WorldContentSession::SaveAll(SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    if(!SavePersistent(scene,renderer,roots,error)) return false;
    for(const auto& region:m_Regions) if(region.loaded&&!SaveRegion(region.record.id,scene,renderer,roots,error)) return false;
    return true;
}
bool WorldContentSession::IsDirty(const std::string& id,SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    try {
        ContentDocument content;
        if(id==Id()) { auto document=m_Document; if(!Capture(id,scene,renderer,roots,document.persistent,error)) return true;
            error.clear(); return WorldText(document)!=m_SavedWorld; }
        const auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==id; });
        Check(found!=m_Regions.end(),"Unknown region."); if(!found->loaded) { error.clear(); return false; }
        if(!Capture(id,scene,renderer,roots,content,error)) return true;
        Check(found->baseline.has_value(),"Loaded region baseline missing.");
        error.clear(); return !RegionalContentEqual(Canonical(std::move(content)),*found->baseline);
    } catch(const std::exception& e) { error=e.what(); return true; }
}
bool WorldContentSession::HasUnsavedChanges(SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    if(IsDirty(Id(),scene,renderer,roots,error)) return true;
    for(const auto& region:m_Regions) if(IsDirty(region.record.id,scene,renderer,roots,error)) return true;
    return false;
}

bool WorldContentSession::CreateRegion(const std::string& name,const cy::Vec3f& minimum,const cy::Vec3f& maximum,
    SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& id,std::string& error)
{
    return Guard(error,[&] {
        Check(Ready()&&scene.contentWorld==Id(),"No active world.");
        auto document=m_Document; auto regions=m_Regions;
        ContentRegionRecord record; record.id=NewContentGuid(); record.name=name; record.minimum=minimum; record.maximum=maximum;
        const auto file=m_File.parent_path()/"Regions"/(record.id+".maixregion"); record.source=MakeSceneAssetPath(file,roots);
        document.regions.push_back(record); Check(Capture(Id(),scene,renderer,roots,document.persistent,error),error);
        auto worldText=WorldText(document); RegionContentDocument region{Id(),record.id,{}};
        regions.push_back({record,true,{},ContentDocument{}}); id=record.id;
        std::filesystem::create_directories(file.parent_path());
        Check(WriteRegionContentDocument(file,region,error,false),error);
        if(!WriteWorldContentDocument(m_File,document,error)) {
            std::error_code ignored; std::filesystem::remove(file,ignored); throw std::runtime_error(error);
        }
        m_Document=std::move(document); m_Regions.swap(regions); m_SavedWorld=std::move(worldText);
    });
}

bool WorldContentSession::DescribeRegionLoad(const std::string& id,const SceneAssetRoots& roots,RegionLoadDescription& output,std::string& error) const
{
    return Guard(error,[&] {
        Check(Ready(),"No active world.");
        const auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==id; });
        Check(found!=m_Regions.end()&&!found->loaded,"Region unknown or already loaded.");
        RegionLoadDescription description; description.world=Id(); description.id=id;
        description.file=ResolveSceneAssetPath(found->record.source,roots);
        for(const auto& key:m_Document.persistent.materials) description.materialOwners.emplace(key.guid,Id());
        for(const auto& region:m_Regions) if(!region.loaded&&region.record.id!=id)
            description.otherFiles.emplace_back(region.record.id,ResolveSceneAssetPath(region.record.source,roots));
        output=std::move(description);
    });
}
bool ReadRegionLoadDocument(const RegionLoadDescription& description,RegionLoadDocument& output,std::string& error)
{
    return Guard(error,[&] {
        RegionLoadDocument loaded; loaded.materialOwners=description.materialOwners;
        for(const auto& source:description.otherFiles) {
            RegionContentDocument other; Check(ReadRegionContentDocument(source.second,other,error),error);
            Check(other.world==description.world&&other.id==source.first,"Other region identity changed.");
            for(const auto& key:other.content.materials) if(!key.shared)
                Check(loaded.materialOwners.emplace(key.guid,source.first).second,"Duplicate material GUID in region files.");
        }
        Check(ReadRegionContentDocument(description.file,loaded.region,error),error);
        Check(loaded.region.world==description.world&&loaded.region.id==description.id,"Region belongs to another world or manifest entry.");
        // Shared materials may have been added since the last world save. Resolve those against live values on the context thread.
        auto owners=loaded.materialOwners;
        for(const auto& key:loaded.region.content.materials) if(!key.shared)
            Check(owners.emplace(key.guid,description.id).second,"Duplicate regional material GUID.");
        loaded.baseline=Canonical(loaded.region.content);
        output=std::move(loaded);
    });
}
bool WorldContentSession::RegionOptions(const RegionLoadDocument& loaded,const SceneState& scene,SceneContentOptions& options,std::string& error) const
{
    return Guard(error,[&] {
        const auto& document=loaded.region;
        Check(Ready()&&document.world==Id()&&scene.contentWorld==Id(),"Region belongs to an inactive world.");
        auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==document.id; });
        Check(found!=m_Regions.end()&&!found->loaded,"Region unknown or already active.");
        auto owners=loaded.materialOwners;
        for(const auto& material:scene.contentMaterials) {
            auto old=owners.find(material.guid);
            Check(old==owners.end()||old->second==material.region,"Live material ownership changed."); owners[material.guid]=material.region;
        }
        ValidateMaterialKeys(document.content,Id(),owners,document.id);
        FillOptions(document.content,document.id,scene,options);
    });
}
bool WorldContentSession::CommitRegion(const RegionLoadDocument& loaded,PreparedSceneContent& prepared,SceneState& scene,Renderer& renderer,std::string& error)
{
    return Guard(error,[&] {
        const auto& document=loaded.region; const auto& id=document.id;
        SceneContentOptions current; Check(RegionOptions(loaded,scene,current,error),error);
        for(const auto& entry:current.borrowedMaterials) {
            Renderer::MaterialSnapshot snapshot;
            Check(prepared.bindings.materials.at(entry.first).id==entry.second.id&&renderer.GetMaterialSnapshot(entry.second,snapshot),"Shared material changed while loading; retry region.");
        }
        Check(RefreshCatalog(scene,error),error);
        auto catalog=m_Catalog; catalog.erase(std::remove_if(catalog.begin(),catalog.end(),[&](const auto& d) { return d.region==id; }),catalog.end());
        AddDescriptors(document.content,id,catalog);
        auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==id; });
        auto baseline=loaded.baseline;
        World world; Check(scene.world.PrepareAppend(prepared.scene.world,world,error),error);
        auto models=scene.models; auto lights=scene.lights; auto materials=scene.sceneMaterials; auto identities=scene.contentMaterials;
        models.insert(models.end(),prepared.scene.models.begin(),prepared.scene.models.end());
        lights.insert(lights.end(),prepared.scene.lights.begin(),prepared.scene.lights.end());
        for(auto handle:prepared.scene.sceneMaterials) Add(materials,handle);
        TrackPrepared(document.content,id,prepared,identities);
        RenderScene renderScene; Check(renderer.PrepareSceneUpdate(prepared.primitives,prepared.lights,{},{},renderScene,error),error);
        renderer.CommitSceneUpdate(std::move(renderScene)); scene.world=std::move(world); scene.models.swap(models); scene.lights.swap(lights);
        scene.sceneMaterials.swap(materials); scene.contentMaterials.swap(identities); scene.nextModelId=std::max(scene.nextModelId,prepared.scene.nextModelId);
        prepared.Release(); found->loaded=true; found->baseline=std::move(baseline); found->lastError.clear(); m_Catalog.swap(catalog);
    });
}
bool WorldContentSession::LoadRegion(const std::string& id,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{
    RegionLoadDescription description; RegionLoadDocument loaded; SceneContentOptions options;
    if(!DescribeRegionLoad(id,roots,description,error)||!ReadRegionLoadDocument(description,loaded,error)||!RegionOptions(loaded,scene,options,error)) return false;
    std::unique_ptr<PreparedSceneContent> prepared;
    if(!PrepareSceneContent(loaded.region.content.scene,renderer,roots,options,prepared,error)) return false;
    return CommitRegion(loaded,*prepared,scene,renderer,error);
}

bool WorldContentSession::UnloadRegion(const std::string& id,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,std::string& error)
{ return RemoveRegion(id,scene,renderer,roots,nullptr,error); }
bool WorldContentSession::DetachRegion(const std::string& id,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,SceneContentRetirement& output,std::string& error)
{ return RemoveRegion(id,scene,renderer,roots,&output,error); }
bool WorldContentSession::RemoveRegion(const std::string& id,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,SceneContentRetirement* output,std::string& error)
{
    return Guard(error,[&] {
        Check(Ready()&&scene.contentWorld==Id()&&id!=Id(),"Persistent content cannot be unloaded as a region.");
        auto found=std::find_if(m_Regions.begin(),m_Regions.end(),[&](const auto& r) { return r.record.id==id; });
        Check(found!=m_Regions.end()&&found->loaded,"Region is unknown or not loaded.");
        Check(scene.world.ContentRegionPins(id)==0,"Region is referenced by Undo/Redo history. Explicitly clear this region's history before unloading.");
        Check(!IsDirty(id,scene,renderer,roots,error),error.empty()?"Save the region before unloading.":error);
        Check(RefreshCatalog(scene,error),error);
        std::vector<ActorId> actors; Renderer::SceneResourceSnapshot resources;
        for(const auto& actor:scene.world.Actors()) if(actor.contentRegion==id) actors.push_back(actor.id);
        auto contains=[&](ActorId actor) { return std::find(actors.begin(),actors.end(),actor)!=actors.end(); };
        for(const auto& model:scene.models) if(contains(model.actor)) {
            for(auto mesh:model.meshes) Add(resources.meshes,mesh);
            for(const auto& section:model.sections) resources.primitives.push_back(section.primitiveId);
        }
        for(const auto& light:scene.lights) if(contains(light.actor)) resources.lights.push_back(light.proxy.id);
        const auto live=renderer.GetMaterialHandles();
        for(const auto& entry:scene.contentMaterials) if(entry.region==id&&Has(live,entry.handle)) Add(resources.materials,entry.handle);
        for(auto mesh:resources.meshes) Check(!renderer.IsMeshRetained(mesh),"A deleted object still retains region geometry.");
        for(auto material:resources.materials) Check(!renderer.IsMaterialRetained(material),"A deleted object still retains a region material.");
        for(const auto& model:scene.models) if(!contains(model.actor)) {
            for(auto mesh:model.meshes) Check(!Has(resources.meshes,mesh),"Another region references this region's geometry.");
            for(const auto& section:model.sections) Check(!Has(resources.materials,section.material),"Another region references this region's material.");
        }
        for(auto material:live) if(!Has(resources.materials,material))
            Check(!Has(resources.materials,renderer.GetMaterialParent(material)),"A material outside this region depends on its parent.");
        World world; Check(scene.world.PrepareRemove(actors,world,error),error);
        if(world.ContentRegion()==id) Check(world.SetContentRegion(Id(),error),error);
        auto models=scene.models; auto lights=scene.lights; auto materials=scene.sceneMaterials; auto identities=scene.contentMaterials;
        models.erase(std::remove_if(models.begin(),models.end(),[&](const auto& m) { return contains(m.actor); }),models.end());
        lights.erase(std::remove_if(lights.begin(),lights.end(),[&](const auto& l) { return contains(l.actor); }),lights.end());
        materials.erase(std::remove_if(materials.begin(),materials.end(),[&](auto h) { return Has(resources.materials,h); }),materials.end());
        identities.erase(std::remove_if(identities.begin(),identities.end(),[&](const auto& m) { return m.region==id; }),identities.end());
        RenderScene renderScene; Check(renderer.PrepareSceneUpdate({},{},resources.primitives,resources.lights,renderScene,error),error);
        for(auto mesh:resources.meshes) Check(!renderScene.HasMeshReference(mesh.id),"A primitive outside this region still references its geometry (including hidden objects).");
        for(auto material:resources.materials) Check(!renderScene.HasMaterialReference(material.id),"A primitive outside this region still references its material (including hidden objects).");
        renderer.CommitSceneUpdate(std::move(renderScene)); scene.world=std::move(world); scene.models.swap(models); scene.lights.swap(lights);
        scene.sceneMaterials.swap(materials); scene.contentMaterials.swap(identities); found->loaded=false;
        found->baseline.reset(); // Unloaded content retains descriptors only.
        if(output) {
            output->meshes=std::move(resources.meshes); output->materials=std::move(resources.materials);
            std::stable_sort(output->materials.begin(),output->materials.end(),[&](auto a,auto b) { return renderer.IsMaterialInstance(a)>renderer.IsMaterialInstance(b); });
        } else Release(resources,renderer);
    });
}
