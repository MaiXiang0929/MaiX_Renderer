// SPDX-License-Identifier: MIT
#include "Editor/Session/DefaultProjectScene.h"
#include "Engine/Runtime/Scene/StartupScene.h"
#include "Engine/Asset/Importer/FbxModelImporter.h"
#include <stdexcept>
bool BuildDefaultProjectScene(const std::filesystem::path& engineAssets,SceneDocument& output,std::string& error)
{
    try
    {
        std::string diagnostic;
        const auto definition=LoadStartupScene(engineAssets/"scenes/startup.json",diagnostic);
        if(!definition.basicScene) throw std::runtime_error("Default Ground/Monkey scene definition is unavailable.");
        const auto& basic=*definition.basicScene;
        SceneDocument d;
        for(const auto* entry:{&basic.ground,&basic.monkey})
        {
            const auto file=std::filesystem::weakly_canonical(engineAssets.parent_path()/entry->path);
            auto result=AssetImport::FbxModelImporter::Import(file);
            if(!result) throw std::runtime_error(result.error);
            SceneActorRecord actor; actor.id=d.actors.size()+1; actor.name=entry->name; actor.local=entry->transform;
            actor.model.emplace(); auto& model=*actor.model;
            model.source=MakeSceneAssetPath(file,{{},engineAssets,{}});
            const auto materialOffset=d.materials.size();
            for(const auto& imported:result.model.materials)
            {
                SceneMaterialRecord m; m.id=d.materials.size()+1; m.name=imported.name;
                m.properties.baseColor=entry->baseColor; m.properties.roughness=entry->roughness;
                if(imported.baseColorTexture!=AssetImport::InvalidImportedTextureIndex)
                {
                    const auto& image=result.model.textures.at(imported.baseColorTexture);
                    auto& t=m.textures[0]; t.source=MakeSceneAssetPath(image.embedded?file:image.sourcePath,{{},engineAssets,{}});
                    t.embeddedId=image.embedded?image.sourceId:0;
                }
                d.materials.push_back(std::move(m));
            }
            for(const auto& mesh:result.model.meshes)
            {
                SceneSectionRecord s; s.mesh=model.meshNames.size(); s.name=mesh.name;
                s.material=materialOffset+mesh.materialIndex+1; s.local=mesh.localToWorld;
                s.bounds.center=mesh.boundsCenter; s.bounds.radius=mesh.boundsRadius;
                model.meshNames.push_back(mesh.name); model.sections.push_back(std::move(s));
            }
            if(entry==&basic.monkey)
            {
                d.defaultModel=actor.id; d.defaultBoundsMin=result.model.boundsMin; d.defaultBoundsMax=result.model.boundsMax;
                const auto size=result.model.boundsMax-result.model.boundsMin;
                d.modelDiameter=size.Length(); d.sceneRadius=std::max(size.Length()*0.5f,1.0f);
                d.objCenter=(result.model.boundsMin+result.model.boundsMax)*0.5f+entry->transform.position;
            }
            d.actors.push_back(std::move(actor));
        }
        SceneActorRecord light; light.id=3; light.name=basic.pointLightName;
        light.local.position=basic.pointLightPosition; light.light.emplace();
        light.light->type=LightType::Point; light.light->color=basic.pointLightColor;
        light.light->intensity=basic.pointLightIntensity; light.light->range=basic.pointLightRange; light.light->castsShadow=false;
        d.mainLight=3; d.actors.push_back(std::move(light));
        SceneActorRecord camera; camera.id=4; camera.name=definition.camera.name;
        camera.local=definition.camera.transform; camera.camera=definition.camera;
        d.actors.push_back(std::move(camera)); d.groundY=basic.ground.transform.position.y;
        if(!ValidateSceneDocument(d,error)) return false;
        output=std::move(d); error.clear(); return true;
    }
    catch(const std::exception& e) { error=e.what(); return false; }
}
