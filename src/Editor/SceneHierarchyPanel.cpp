// SPDX-License-Identifier: MIT
#include "SceneHierarchyPanel.h"

#include <string>

#include <imgui.h>

#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"

namespace
{
void SetEditorPanelPosition(float width, float offset)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float right = viewport->WorkPos.x + viewport->WorkSize.x;
    const float x = right - width - offset;
    ImGui::SetNextWindowPos(
        ImVec2(x < viewport->WorkPos.x + 8.0f
            ? viewport->WorkPos.x + 8.0f
            : x,
            viewport->WorkPos.y + 16.0f),
        ImGuiCond_FirstUseEver);
}
}

void SceneHierarchyPanel::Draw(
    EditorSelection& selection,
    const EditableModel& model,
    const std::vector<EditableLight>& lights)
{
    SetEditorPanelPosition(300.0f, 380.0f);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene"))
    {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Objects");
    if (!model.IsValid())
    {
        ImGui::TextUnformatted("No model loaded.");
    }
    else
    {
        const std::string label = model.name.empty()
            ? "Model"
            : model.name;
        const std::string selectableLabel = label + "##ActiveModel";
        if (ImGui::Selectable(
                selectableLabel.c_str(), selection.IsModelSelected()))
        {
            selection.SelectModel();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%zu sections", model.sections.size());
    }

    if (ImGui::TreeNodeEx(
            "Lights",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        if (lights.empty())
        {
            ImGui::TextUnformatted("No lights.");
        }
        for (const EditableLight& light : lights)
        {
            if (!light.IsValid())
                continue;

            const std::string label = light.name.empty()
                ? "Light"
                : light.name;
            const std::string selectableLabel =
                label + "##Light" + std::to_string(light.proxy.id);
            if (ImGui::Selectable(
                    selectableLabel.c_str(),
                    selection.IsLightSelected(light.proxy.id)))
            {
                selection.SelectLight(light.proxy.id);
            }
            ImGui::SameLine();
            const char* typeName = light.proxy.type == LightType::Directional
                ? "Directional"
                : light.proxy.type == LightType::Spot
                    ? "Spot"
                    : "Point";
            ImGui::TextDisabled("%s", typeName);
        }
        ImGui::TreePop();
    }

    ImGui::End();
}
