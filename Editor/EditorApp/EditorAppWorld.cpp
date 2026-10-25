// SPDX-License-Identifier: MIT
#include "Editor/EditorApp/EditorApp.h"
#include "Editor/Session/EditorProject.h"
#include "Editor/Session/EditorWorkspace.h"
#include "Editor/ContentBrowser/AssetImportPanel.h"
#include "Editor/Viewport/EditorViewportController.h"
#include "Engine/Runtime/Engine/SceneSubtree.h"
#include "Engine/Core/Platform/Windows/FileDialog.h"
#include "Engine/Core/Platform/Windows/PlatformPaths.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <algorithm>

namespace
{
SceneAssetRoots ContentRoots(const EditorProject& project)
{ return {project.AssetsRoot(),PlatformPaths::ExecutableDirectory()/"assets",std::filesystem::path(MAIX_ENGINE_SOURCE_DIR)/"assets"}; }
}
void EditorApp::CreateWorldFile(void* owner)
{
    if (!m_Project->IsReady()||m_Engine.Content().Ready()) return;
    const auto file=FileDialog::SaveWorldFile(owner,m_Project->AssetsRoot());
    if (!file) return;
    m_History.Finish(m_Engine.Scene().world); std::string error;
    if (!m_Engine.Content().Create(*file,m_Engine.Scene(),*m_Renderer,ContentRoots(*m_Project),error))
    { ReportSceneFileError(error); return; }
    // Successful conversion ends legacy history whose actions predate region ownership.
    m_History.Clear(); m_WorldPanel={}; m_WorldAction={};
    m_Workspace->AddLog("World created: "+file->u8string());
}
bool EditorApp::OpenWorldFile(const std::filesystem::path& file)
{
    std::string error;
    if (!m_Engine.Content().Open(file,m_Engine.Scene(),*m_Renderer,ContentRoots(*m_Project),error))
    { ReportSceneFileError(error); return false; }
    ResetSceneEditorState(); m_WorldPanel={}; m_WorldAction={};
    m_ScenePath.clear(); m_LastSavedSceneText.clear();
    m_Workspace->AddLog("World opened: "+file.u8string()); return true;
}
void EditorApp::HandleWorldContentAction()
{
    if (m_WorldAction.kind==WorldContentActionKind::None||m_AssetImportPanel->IsBusy()||m_ConfirmSceneReplacement) return;
    auto action=std::move(m_WorldAction); m_WorldAction={};
    if (!m_Engine.Content().Ready()) return;
    auto& scene=m_Engine.Scene(); auto& content=m_Engine.Content();
    m_History.Finish(scene.world); std::string error; bool success=false;
    try {
        const auto roots=ContentRoots(*m_Project);
        switch (action.kind) {
        case WorldContentActionKind::CreateRegion: {
            std::string id;
            success=content.CreateRegion(action.name,action.minimum,action.maximum,scene,*m_Renderer,roots,id,error);
            if(success) success=content.SetEditingRegion(id,scene,error);
            break;
        }
        case WorldContentActionKind::Load: success=content.LoadRegion(action.region,scene,*m_Renderer,roots,error); break;
        case WorldContentActionKind::Unload: success=content.UnloadRegion(action.region,scene,*m_Renderer,roots,error); break;
        case WorldContentActionKind::Save: success=content.SaveRegion(action.region,scene,*m_Renderer,roots,error); break;
        case WorldContentActionKind::Edit: success=content.SetEditingRegion(action.region,scene,error); break;
        case WorldContentActionKind::DiscardHistory: m_History.DiscardRegion(action.region); success=true; break;
        case WorldContentActionKind::BindShared: {
            auto* model=FindSceneModel(scene.models,action.model);
            if(!model||!content.CanBindMaterial(scene,model->actor,action.replacement)) { error="Shared material or model is unavailable."; break; }
            std::vector<PrimitiveId> primitives; auto materials=model->materials;
            for(const auto& section:model->sections) if(section.material.id==action.material.id) primitives.push_back(section.primitiveId);
            for(auto handle=action.replacement;handle.IsValid();handle=m_Renderer->GetMaterialParent(handle))
                if(std::none_of(materials.begin(),materials.end(),[&](auto m) { return m.id==handle.id; })) materials.push_back(handle);
            if(primitives.empty()||!m_Renderer->ReplacePrimitiveMaterials(primitives,action.material,action.replacement)) { error="Material binding changed before this operation."; break; }
            model->materials.swap(materials);
            for(auto& section:model->sections) if(section.material.id==action.material.id) section.material=action.replacement;
            ReleaseUnusedModelMaterial(scene,*m_Renderer,model->id,action.material);
            m_MaterialSelection.Select(model->id,action.replacement); success=true; break;
        }
        default: break;
        }
    } catch(const std::exception& e) { error=e.what(); }
    if(!success) { ReportSceneFileError("[World] "+error); return; }
    if(!m_EditorSelection.ResolveActor(scene).IsValid()) m_EditorSelection.Clear();
    if(!m_PendingFocusSelection.ResolveActor(scene).IsValid()) m_PendingFocusSelection.Clear();
    m_MaterialSelection.Synchronize(m_EditorSelection,scene.models,GetSceneMaterialHandles(scene,*m_Renderer));
    m_Workspace->AddLog("World operation completed.");
}
