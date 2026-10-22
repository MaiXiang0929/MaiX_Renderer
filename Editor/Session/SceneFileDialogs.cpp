// SPDX-License-Identifier: MIT
#include "Editor/Session/SceneFileDialogs.h"
#include <imgui.h>
void DrawSceneFileDialogs(bool& requestReplacement,bool replacementPending,std::string& error,
    const std::function<bool()>& save,const std::function<void()>& open,const std::function<void()>& cancel,const char* replacementTitle)
{
    if (requestReplacement) { ImGui::OpenPopup(replacementTitle); requestReplacement=false; }
    if (ImGui::BeginPopupModal(replacementTitle,nullptr,ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("The current scene has unsaved changes.");
        ImGui::TextUnformatted("Save them before continuing?");
        if (!error.empty())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize()*35);
            ImGui::TextUnformatted(error.c_str()); ImGui::PopTextWrapPos();
        }
        if (ImGui::Button("Save and Continue")&&save()) { open(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (ImGui::Button("Discard and Continue")) { open(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) { cancel(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    // A sibling popup would replace the confirmation and strand the pending path.
    if (!replacementPending&&!error.empty()) ImGui::OpenPopup("Scene file error");
    if (ImGui::BeginPopupModal("Scene file error",nullptr,ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*35); ImGui::TextUnformatted(error.c_str()); ImGui::PopTextWrapPos();
        if (ImGui::Button("OK")) { error.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
