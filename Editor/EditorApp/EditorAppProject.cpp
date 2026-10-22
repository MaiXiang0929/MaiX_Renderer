// SPDX-License-Identifier: MIT
#include "Editor/EditorApp/EditorApp.h"
#include "Editor/Session/EditorProject.h"
#include "Editor/Session/EditorWorkspace.h"
#include "Editor/Session/ProjectSession.h"
#include "Editor/Session/DefaultProjectScene.h"
#include "Engine/Runtime/Engine/ScenePersistence.h"
#include "Engine/Core/Platform/Windows/FileDialog.h"
#include "Engine/Core/Platform/Windows/PlatformPaths.h"
#include <imgui.h>
#include <iostream>

bool EditorApp::OpenProjectFile(const std::filesystem::path& path)
{
    try
    {
        const SceneAssetRoots roots{{},PlatformPaths::ExecutableDirectory()/"assets",
            std::filesystem::path(MAIX_ENGINE_SOURCE_DIR)/"assets"};
        std::string error;
        if (!OpenProjectSession(*m_Project,m_Engine.Scene(),*m_Renderer,roots,path,error))
        { ReportSceneFileError(error); return false; }
        ResetSceneEditorState();
        m_Workspace->SynchronizeProject();
        m_ScenePath=m_Project->DefaultScene(); m_SceneProjectRoot=m_Project->Root();
        m_LastSavedSceneText.clear(); SceneDocument normalized;
        if (CaptureSceneDocument(m_Engine.Scene(),*m_Renderer,
            {m_Project->AssetsRoot(),roots.engineAssets,roots.engineSourceAssets},normalized,error))
            EncodeSceneDocument(normalized,m_LastSavedSceneText,error);
        const auto message="Project opened: "+m_Project->ProjectFile().u8string();
        std::cout<<"[Project] "<<message<<std::endl; m_Workspace->AddLog(message);
        return true;
    }
    catch (const std::exception& e) { ReportSceneFileError(e.what()); return false; }
}

void EditorApp::ExecuteProjectAction(ProjectAction action)
{
    if (action.kind==SceneFileAction::OpenProject) { OpenProjectFile(action.path); return; }
    try
    {
        SceneDocument initial; ProjectDocument created; std::string error;
        // Choosing an existing scene does not require the default template assets.
        if (action.existingScene.empty()&&!BuildDefaultProjectScene(
            PlatformPaths::ExecutableDirectory()/"assets",initial,error))
        { ReportSceneFileError(error); return; }
        const bool saved=action.kind==SceneFileAction::NewProject?
            CreateProjectDocument(action.path,action.name,initial,m_Project->ProtectedDirectories(),created,error):
            InitializeLegacyProject(action.path,action.existingScene,initial,m_Project->ProtectedDirectories(),created,error);
        if (!saved) { ReportSceneFileError(error); return; }
        if (!OpenProjectFile(created.file))
            ReportSceneFileError("Project created at "+created.file.u8string()+
                ", but opening failed. The current scene is retained. "+m_SceneFileError);
    }
    catch (const std::exception& e) { ReportSceneFileError(e.what()); }
}

void EditorApp::DrawProjectCreationDialogs(void* owner)
{
    if (m_RequestNewProjectDialog) { ImGui::OpenPopup("New Project"); m_RequestNewProjectDialog=false; }
    if (ImGui::BeginPopupModal("New Project",nullptr,ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Choose the project name. A new folder will be created in:");
        ImGui::TextUnformatted(m_NewProjectParent->u8string().c_str());
        ImGui::InputText("Name",m_NewProjectName,sizeof(m_NewProjectName));
        if (ImGui::Button("Create",{0,0})&&m_NewProjectName[0])
        {
            m_PendingProjectAction=ProjectAction{SceneFileAction::NewProject,*m_NewProjectParent,{},m_NewProjectName};
            m_NewProjectParent.reset(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { m_NewProjectParent.reset(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (m_RequestLegacyProjectDialog) { ImGui::OpenPopup("Initialize Project Folder"); m_RequestLegacyProjectDialog=false; }
    if (ImGui::BeginPopupModal("Initialize Project Folder",nullptr,ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(m_LegacyProjectRoot->u8string().c_str());
        ImGui::TextUnformatted("Create a .maixproject descriptor in this existing folder.");
        ImGui::TextUnformatted("Select its initial scene, or create Assets/Scenes/Main.maixscene.");
        ImGui::TextUnformatted("Existing files will not be overwritten.");
        if (ImGui::Button("Use Existing Scene..."))
        {
            const auto scene=FileDialog::OpenSceneFile(owner,*m_LegacyProjectRoot/"Assets");
            if (scene)
            {
                m_PendingProjectAction=ProjectAction{SceneFileAction::InitializeProject,*m_LegacyProjectRoot,*scene,{}};
                m_LegacyProjectRoot.reset(); ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Default Scene"))
        {
            m_PendingProjectAction=ProjectAction{SceneFileAction::InitializeProject,*m_LegacyProjectRoot,{},{}};
            m_LegacyProjectRoot.reset(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { m_LegacyProjectRoot.reset(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
