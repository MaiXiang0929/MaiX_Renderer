// SPDX-License-Identifier: MIT
#include "Editor/SceneHierarchy/SceneHierarchyPanel.h"

#include <string>

#include <imgui.h>
#include "Editor/Session/EditorWidgets.h"

#include "Engine/Runtime/Scene/SceneLight.h"
#include "Engine/Runtime/Scene/SceneModel.h"

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

ModelId SceneHierarchyPanel::Draw(
    EditorSelection& selection,
    const std::vector<SceneModel>& models,
    const std::vector<SceneLight>& lights, const SceneCamera* camera)
{
    SetEditorPanelPosition(300.0f, 380.0f);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene"))
    {
        ImGui::End();
        return {};
    }

    ImGui::SeparatorText("Objects");
    if (models.empty())
    {
        ImGui::TextUnformatted("No model loaded.");
    }
    else
    {
        for (const SceneModel& model : models)
        {
            const std::string label = model.name.empty() ? "Model" : model.name;
            const std::string selectableLabel = "##Model" + std::to_string(model.id.value);
            if (EditorWidgets::IconSelectable(selectableLabel.c_str(),EditorWidgets::Icon::Mesh,
                    label.c_str(), selection.IsModelSelected(model.id)))
                selection.SelectModel(model.id);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%zu sections)",label.c_str(),model.sections.size());
        }
    }

    ModelId removedModel;
    if (selection.type == EditorSelectionType::Model &&
        FindSceneModel(models, selection.modelId) != nullptr &&
        ImGui::Button("Remove selected model"))
        removedModel = selection.modelId;

    if (ImGui::TreeNodeEx(
            "Lights",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        if (lights.empty())
        {
            ImGui::TextUnformatted("No lights.");
        }
        for (const SceneLight& light : lights)
        {
            if (!light.IsValid())
                continue;

            const std::string label = light.name.empty()
                ? "Light"
                : light.name;
            const std::string selectableLabel = "##Light" + std::to_string(light.proxy.id);
            if (EditorWidgets::IconSelectable(selectableLabel.c_str(),EditorWidgets::Icon::Light,
                    label.c_str(), selection.IsLightSelected(light.proxy.id)))
            {
                selection.SelectLight(light.proxy.id);
            }
            const char* typeName = light.proxy.type == LightType::Directional
                ? "Directional"
                : light.proxy.type == LightType::Spot
                    ? "Spot"
                    : "Point";
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)",label.c_str(),typeName);
        }
        ImGui::TreePop();
    }

    if (camera && EditorWidgets::IconSelectable("##Camera",EditorWidgets::Icon::Camera,
            camera->name.c_str(), selection.IsCameraSelected(camera->id)))
        selection.SelectCamera(camera->id);
    ImGui::End();
    return removedModel;
}
