// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/ScenePersistence.h"
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

static bool CaptureContent(SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,
    const SceneContentSelection* selection,SceneDocument& output,SceneContentBindings* bindings,std::string& error)
{
    try
    {
        Require(RegisterSceneActors(scene),"Cannot register scene actors before saving.");
        auto resources=renderer.GetSceneResources();
        resources.materials=selection?selection->materials:GetSceneMaterialHandles(scene,renderer);
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
        for (const auto& actor:scene.world.Actors())
            if (!selection || std::find(selection->actors.begin(),selection->actors.end(),actor.id)!=selection->actors.end())
                actors.emplace(actor.id.value,actors.size()+1);
        if (selection) Require(actors.size()==selection->actors.size(),"Missing or duplicate capture actor.");
        for (auto handle:resources.materials)
        {
            if (selection&&std::any_of(selection->sharedMaterials.begin(),selection->sharedMaterials.end(),[&](auto h) { return h.id==handle.id; })) {
                SceneMaterialRecord placeholder; placeholder.id=materials.at(handle.id);
                d.materials.push_back(std::move(placeholder)); continue;
            }
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
                    AssetImport::TextureSource source;
                    if (snapshot.textureRecipes[i]) source=*snapshot.textureRecipes[i];
                    else
                    {
                        Require(!snapshot.textureFiles[i].empty(),"Texture has no reload source: "+snapshot.name+
                            " (generated textures need a supported reconstruction recipe).");
                        source.image={snapshot.textureFiles[i],snapshot.embeddedTextureIds[i]};
                    }
                    const auto check=[](const AssetImport::TextureSourceImage& image) {
                        Require(std::filesystem::is_regular_file(image.path),"Missing texture source: "+image.path.u8string());
                    };
                    if (source.kind==AssetImport::TextureSourceKind::Orm)
                        for (const auto& input:source.ormInputs) { if (input) check(input->image); }
                    else check(source.image);
                    const auto mode=t.mode; t=MakeSceneTextureRecord(source,roots); t.mode=mode;
                }
            }
            d.materials.push_back(std::move(m));
        }
        size_t modelCount=0,lightCount=0,cameraCount=0;
        for (const auto& actor:scene.world.Actors())
        {
            if (!actors.count(actor.id.value)) continue;
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
        Require(selection || (modelCount==scene.models.size()&&lightCount==scene.lights.size()&&cameraCount==(scene.camera.id.IsValid()?1u:0u)),"Unregistered typed scene components.");
        Require(selection || (primitives.size()==resources.primitives.size()&&lightCount==resources.lights.size()),
            "Scene contains renderer-only objects without reload provenance; saving would omit visible content.");
        d.reflectionGroundEnabled=scene.reflectionGroundEnabled; d.groundY=scene.groundY; d.modelDiameter=scene.modelDiameter;
        d.sceneRadius=scene.sceneRadius; d.defaultBoundsMin=scene.defaultBoundsMin; d.defaultBoundsMax=scene.defaultBoundsMax; d.objCenter=scene.objCenter;
        if (selection) {
            if (!std::any_of(d.actors.begin(),d.actors.end(),[&](const auto& a) { return a.id==d.mainLight; })) d.mainLight=0;
            if (!std::any_of(d.actors.begin(),d.actors.end(),[&](const auto& a) { return a.id==d.defaultModel; })) d.defaultModel=0;
        }
        if (!ValidateSceneDocument(d,error)) return false;
        if (bindings) {
            SceneContentBindings captured;
            for (const auto& entry:actors) captured.actors.emplace(entry.second,ActorId{entry.first});
            for (const auto& entry:materials) captured.materials.emplace(entry.second,MaterialHandle{entry.first});
            *bindings=std::move(captured);
        }
        output=std::move(d); error.clear(); return true;
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}

bool ReplaceSceneFromDocument(SceneState& active,Renderer& renderer,const SceneAssetRoots& roots,
                              const SceneDocument& d,std::string& error)
{
    try {
        const auto old=renderer.GetSceneResources();
        std::unique_ptr<PreparedSceneContent> candidate;
        if (!PrepareSceneContent(d,renderer,roots,{},candidate,error)) return false;
        RenderScene renderScene;
        if (!renderer.PrepareSceneUpdate(candidate->primitives,candidate->lights,old.primitives,old.lights,renderScene,error)) return false;
        // Environment is the last fallible operation; every commit below is allocation-free.
        if (!renderer.LoadEnvironment(candidate->scene.environmentSource,error)) return false;
        renderer.CommitSceneUpdate(std::move(renderScene));
        active=std::move(candidate->scene); candidate->Release();
        ReleaseSnapshot(old,renderer); error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}

bool CaptureSceneDocument(SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,SceneDocument& output,std::string& error)
{ return CaptureContent(scene,renderer,roots,nullptr,output,nullptr,error); }
bool CaptureSceneContent(SceneState& scene,const Renderer& renderer,const SceneAssetRoots& roots,
    const SceneContentSelection& selection,SceneDocument& output,SceneContentBindings& bindings,std::string& error)
{ return CaptureContent(scene,renderer,roots,&selection,output,&bindings,error); }
