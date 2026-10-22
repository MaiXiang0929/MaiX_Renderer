// SPDX-License-Identifier: MIT
#include "Editor/Session/ProjectSession.h"
#include "Engine/Runtime/Engine/ScenePersistence.h"
bool OpenProjectSession(EditorProject& project,SceneState& scene,Renderer& renderer,
    const SceneAssetRoots& engineRoots,const std::filesystem::path& file,std::string& error)
{
    ProjectDocument candidate;
    if(!project.PrepareOpen(file,candidate,error)) return false;
    SceneDocument content;
    if(!ReadSceneDocument(candidate.sceneFile,content,error)) return false;
    SceneAssetRoots roots{candidate.assets,engineRoots.engineAssets,engineRoots.engineSourceAssets};
    if(!ReplaceSceneFromDocument(scene,renderer,roots,content,error)) return false;
    // All validation and allocation precedes the project identity commit.
    project.Activate(std::move(candidate)); return true;
}
