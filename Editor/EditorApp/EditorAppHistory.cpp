// SPDX-License-Identifier: MIT
#include "Editor/EditorApp/EditorApp.h"
#include "Editor/Session/EditorWorkspace.h"
#include "Editor/ContentBrowser/AssetImportPanel.h"
#include "Editor/Viewport/EditorViewportController.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include <imgui.h>

bool EditorApp::HistoryInputReady() const
{
    return !m_History.IsEditing() && !m_AssetImportPanel->IsBusy() &&
        !m_PendingOpenScene && !m_PendingProjectAction && !m_ConfirmSceneReplacement &&
        !m_NewProjectParent && !m_LegacyProjectRoot;
}
void EditorApp::ReplayHistory(bool redo)
{
    if (!HistoryInputReady()) return;
    std::string error;
    if (!(redo ? m_History.Redo(m_Engine.Scene().world,error) : m_History.Undo(m_Engine.Scene().world,error)))
        m_Workspace->AddLog("[History] "+error);
    else
    {
        SynchronizeSceneActors(m_Engine.Scene(),*m_Renderer);
        if (m_History.RestoredActor().IsValid()) m_EditorSelection.SelectActor(m_History.RestoredActor(),m_Engine.Scene());
        if (!m_EditorSelection.ResolveActor(m_Engine.Scene()).IsValid()) m_EditorSelection.Clear();
        if (!m_PendingFocusSelection.ResolveActor(m_Engine.Scene()).IsValid()) m_PendingFocusSelection.Clear();
        if (!m_Engine.Scene().camera.id.IsValid()) m_SceneCameraView=false;
        m_MaterialSelection.Synchronize(m_EditorSelection,m_Engine.Scene().models,
            GetSceneMaterialHandles(m_Engine.Scene(),*m_Renderer));
    }
}
void EditorApp::DrawEditMenu()
{
    const auto undo = std::string("Undo ")+m_History.UndoLabel();
    const auto redo = std::string("Redo ")+m_History.RedoLabel();
    const bool ready = HistoryInputReady();
    if (ImGui::MenuItem(undo.c_str(),"Ctrl+Z",false,ready && m_History.CanUndo())) ReplayHistory(false);
    if (ImGui::MenuItem(redo.c_str(),"Ctrl+Y / Ctrl+Shift+Z",false,ready && m_History.CanRedo())) ReplayHistory(true);
    ImGui::Separator();
    ImGui::TextDisabled("Objects: transforms, names, visibility, parenting, deletion");
}
void EditorApp::HandleHistoryShortcuts()
{
    const auto& io = ImGui::GetIO();
    if (!HistoryInputReady() || io.WantTextInput || ImGui::IsAnyItemActive() ||
        ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ||
        !io.KeyCtrl || io.KeyAlt || io.KeySuper) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Z,false))
    {
        if (io.KeyShift ? m_History.CanRedo() : m_History.CanUndo()) ReplayHistory(io.KeyShift);
    }
    else if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y,false) && m_History.CanRedo()) ReplayHistory(true);
}
void EditorApp::InvalidateEditorHistory(const char* reason)
{
    m_History.Clear();
    m_MainLightDrag=false;
    if (m_ViewportController) m_ViewportController->CancelPointerInput();
    if (m_Workspace) m_Workspace->AddLog(std::string("[History] Cleared: ")+reason+" (operation has no Undo support).");
}
