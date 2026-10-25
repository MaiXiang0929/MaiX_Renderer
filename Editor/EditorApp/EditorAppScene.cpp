// SPDX-License-Identifier: MIT
#include "Editor/EditorApp/EditorApp.h"
#include "Editor/Session/EditorWorkspace.h"
#include "Editor/Session/EditorProject.h"
#include "Editor/Session/SceneFileDialogs.h"
#include "Editor/ContentBrowser/AssetImportPanel.h"
#include "Editor/Viewport/EditorViewportController.h"
#include "Engine/Runtime/Engine/ScenePersistence.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Core/Platform/Windows/FileDialog.h"
#include "Engine/Core/Platform/Windows/PlatformPaths.h"
#include <imgui.h>
#include <iostream>

namespace
{
SceneAssetRoots Roots(const EditorProject& project)
{ return {project.AssetsRoot(),PlatformPaths::ExecutableDirectory()/"assets",std::filesystem::path(MAIX_ENGINE_SOURCE_DIR)/"assets"}; }
}

void EditorApp::DrawSceneFileMenu()
{
    const bool idle=!m_AssetImportPanel->IsBusy()&&!m_PendingOpenScene&&!m_PendingProjectAction&&
        !m_NewProjectParent&&!m_LegacyProjectRoot;
    if (ImGui::MenuItem("New Project...",nullptr,false,idle)) m_SceneFileAction=SceneFileAction::NewProject;
    if (ImGui::MenuItem("Open Project...",nullptr,false,idle)) m_SceneFileAction=SceneFileAction::OpenProject;
    if (ImGui::MenuItem("Initialize Project Folder...",nullptr,false,idle)) m_SceneFileAction=SceneFileAction::InitializeProject;
    ImGui::Separator();
    const bool ready=m_Project->IsReady()&&idle;
    if (ImGui::MenuItem("Open Scene...",nullptr,false,ready)) m_SceneFileAction=SceneFileAction::Open;
    if (ImGui::MenuItem(m_Engine.Content().Ready()?"Save World and Loaded Regions":"Save Scene",nullptr,false,ready)) m_SceneFileAction=SceneFileAction::Save;
    if (ImGui::MenuItem("Save Scene As...",nullptr,false,ready&&!m_Engine.Content().Ready())) m_SceneFileAction=SceneFileAction::SaveAs;
    ImGui::Separator();
    if (ImGui::MenuItem("Create World From Scene...",nullptr,false,ready&&!m_Engine.Content().Ready())) m_SceneFileAction=SceneFileAction::NewWorld;
    if (ImGui::MenuItem("Open World...",nullptr,false,ready)) m_SceneFileAction=SceneFileAction::OpenWorld;
    ImGui::TextDisabled("Project: %s",m_Project->Name().c_str());
    ImGui::TextDisabled("Root: %s",m_Project->Root().u8string().c_str());
    if (m_Engine.Content().Ready()) ImGui::TextDisabled("World: %s",m_Engine.Content().File().u8string().c_str());
    else if (!m_ScenePath.empty()) ImGui::TextDisabled("Scene: %s",m_ScenePath.u8string().c_str());
}

void EditorApp::ReportSceneFileError(std::string error)
{
    m_SceneFileError=std::move(error);
    std::cerr<<"[Scene] "<<m_SceneFileError<<'\n';
    m_Workspace->AddLog("[Scene] "+m_SceneFileError);
}

bool EditorApp::SaveSceneFile(bool saveAs,void* owner)
{
    m_History.Finish(m_Engine.Scene().world);
    if (m_Engine.Content().Ready()) {
        std::string error;
        if (!m_Engine.Content().SaveAll(m_Engine.Scene(),*m_Renderer,Roots(*m_Project),error)) { ReportSceneFileError(error); return false; }
        m_Workspace->AddLog("World and loaded regions saved."); return true;
    }
    if (!m_Project->IsReady())
    {
        ReportSceneFileError("No project Assets directory is available. To switch projects, choose Discard and Continue, or Cancel and open a valid project.");
        return false;
    }
    try
    {
        SceneDocument document; std::string error,text;
        if (!CaptureSceneDocument(m_Engine.Scene(),*m_Renderer,Roots(*m_Project),document,error)||
            !EncodeSceneDocument(document,text,error))
        { ReportSceneFileError(error); return false; }
        auto path=m_ScenePath;
        if (saveAs||path.empty()||m_SceneProjectRoot!=m_Project->Root())
        {
            const auto directory=m_Project->AssetsRoot()/"Scenes";
            std::filesystem::create_directories(directory);
            const auto selected=FileDialog::SaveSceneFile(owner,directory);
            if (!selected) return false;
            path=*selected;
        }
        if (path.extension()!=L".maixscene")
        { ReportSceneFileError("Scene files must use the .maixscene extension."); return false; }
        if (!m_Project->ContainsAssetPath(path))
        { ReportSceneFileError("Save the scene inside: "+m_Project->AssetsRoot().u8string()); return false; }
        if (!WriteSceneDocument(path,document,error)) { ReportSceneFileError(error); return false; }
        m_ScenePath=path; m_SceneProjectRoot=m_Project->Root(); m_LastSavedSceneText=std::move(text);
        m_Workspace->AddLog("Scene saved: "+path.u8string()); return true;
    }
    catch (const std::exception& e) { ReportSceneFileError(e.what()); return false; }
}

bool EditorApp::OpenSceneFile(const std::filesystem::path& path)
{
    if (path.extension()==".maixworld") return OpenWorldFile(path);
    try
    {
        if (!m_Project->ContainsAssetPath(path))
        { ReportSceneFileError("Open a scene inside: "+m_Project->AssetsRoot().u8string()); return false; }
        SceneDocument document; std::string error;
        if (!ReadSceneDocument(path,document,error)||
            !ReplaceSceneFromDocument(m_Engine.Scene(),*m_Renderer,Roots(*m_Project),document,error))
        { ReportSceneFileError(error); return false; }
        ResetSceneEditorState();
        m_Engine.Content().Reset(); m_WorldAction={};
        m_ScenePath=path; m_SceneProjectRoot=m_Project->Root();
        SceneDocument normalized;
        m_LastSavedSceneText.clear();
        if (CaptureSceneDocument(m_Engine.Scene(),*m_Renderer,Roots(*m_Project),normalized,error))
            EncodeSceneDocument(normalized,m_LastSavedSceneText,error);
        m_Workspace->AddLog("Scene opened: "+path.u8string()); return true;
    }
    catch (const std::exception& e) { ReportSceneFileError(e.what()); return false; }
}

void EditorApp::ResetSceneEditorState()
{
    m_History.Clear();
    m_MainLightDrag=false;
    m_EditorSelection.Clear(); m_PendingFocusSelection.Clear();
    m_MaterialSelection=EditorMaterialSelection{}; m_SceneCameraView=false;
    m_ViewportController->CancelPointerInput(&m_History,&m_Engine.Scene().world);
    if (m_Engine.Scene().defaultModelId.IsValid())
    {
        m_EditorSelection.SelectModel(m_Engine.Scene().defaultModelId);
        m_PendingFocusSelection=m_EditorSelection;
    }
}

bool EditorApp::SceneHasUnsavedChanges()
{
    m_History.Finish(m_Engine.Scene().world);
    if (m_Engine.Content().Ready()) {
        std::string error;
        return m_Engine.Content().HasUnsavedChanges(m_Engine.Scene(),*m_Renderer,Roots(*m_Project),error);
    }
    SceneDocument current; std::string text,error;
    return m_LastSavedSceneText.empty()||
        !CaptureSceneDocument(m_Engine.Scene(),*m_Renderer,Roots(*m_Project),current,error)||
        !EncodeSceneDocument(current,text,error)||text!=m_LastSavedSceneText;
}

void EditorApp::HandleSceneFileActions(void* owner)
{
    const bool projectWasPending=m_PendingProjectAction.has_value();
    const auto action=m_SceneFileAction; m_SceneFileAction=SceneFileAction::None;
    if (action==SceneFileAction::NewWorld) CreateWorldFile(owner);
    if (action==SceneFileAction::Save||action==SceneFileAction::SaveAs)
        SaveSceneFile(action==SceneFileAction::SaveAs,owner);
    if (action==SceneFileAction::Open||action==SceneFileAction::OpenWorld)
    {
        const auto path=action==SceneFileAction::OpenWorld?FileDialog::OpenWorldFile(owner,m_Project->AssetsRoot()):FileDialog::OpenSceneFile(owner,m_Project->AssetsRoot()/"Scenes");
        if (path)
        {
            if (!SceneHasUnsavedChanges()) OpenSceneFile(*path);
            else { m_PendingOpenScene=*path; m_ConfirmSceneReplacement=true; }
        }
    }
    if (action==SceneFileAction::OpenProject)
    {
        const auto path=FileDialog::OpenProjectFile(owner,m_Project->Root());
        if (path) m_PendingProjectAction=ProjectAction{action,*path,{},{}};
    }
    if (action==SceneFileAction::NewProject)
    {
        m_NewProjectParent=FileDialog::OpenDirectory(owner,m_Project->Root().parent_path());
        m_RequestNewProjectDialog=m_NewProjectParent.has_value();
    }
    if (action==SceneFileAction::InitializeProject)
    {
        m_LegacyProjectRoot=FileDialog::OpenDirectory(owner,m_Project->Root());
        std::error_code pathError;
        if(m_LegacyProjectRoot&&m_Project->IsReady()&&
            std::filesystem::equivalent(*m_LegacyProjectRoot,m_Project->AssetsRoot(),pathError)&&!pathError)
        {
            ReportSceneFileError("Select the project root: "+m_Project->Root().u8string()+
                "\nThe selected Assets folder is the project's content directory.");
            m_LegacyProjectRoot.reset();
        }
        m_RequestLegacyProjectDialog=m_LegacyProjectRoot.has_value();
    }
    DrawProjectCreationDialogs(owner);
    if (m_PendingProjectAction&&!projectWasPending)
    {
        if (SceneHasUnsavedChanges()) m_ConfirmSceneReplacement=true;
        else
        {
            auto pending=std::move(*m_PendingProjectAction); m_PendingProjectAction.reset();
            ExecuteProjectAction(std::move(pending));
        }
    }
    DrawSceneFileDialogs(m_ConfirmSceneReplacement,m_PendingOpenScene.has_value()||m_PendingProjectAction.has_value(),m_SceneFileError,
        [&] { return SaveSceneFile(false,owner); },
        [&] {
            m_SceneFileError.clear();
            if (m_PendingProjectAction)
            {
                auto pending=std::move(*m_PendingProjectAction); m_PendingProjectAction.reset();
                ExecuteProjectAction(std::move(pending));
            }
            else if (m_PendingOpenScene)
            {
                const auto path=*m_PendingOpenScene; m_PendingOpenScene.reset(); OpenSceneFile(path);
            }
        },
        [&] { m_PendingOpenScene.reset(); m_PendingProjectAction.reset(); m_SceneFileError.clear(); },
        m_PendingProjectAction?"Switch project?":"Open another scene?");
}
