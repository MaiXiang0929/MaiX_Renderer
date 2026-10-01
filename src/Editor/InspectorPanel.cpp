// SPDX-License-Identifier: MIT
#include "InspectorPanel.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"
#include "Editor/EditorMaterialSelection.h"
#include "Editor/EditorValueConstraints.h"
#include "Renderer/Core/Renderer.h"

namespace
{
constexpr float Pi = 3.14159265358979323846f;

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

const char* LightTypeName(LightType type)
{
    switch (type)
    {
    case LightType::Directional: return "Directional";
    case LightType::Spot: return "Spot";
    case LightType::Point: return "Point";
    }
    return "Unknown";
}
}

bool InspectorPanel::Draw(
    EditorSelection& selection,
    std::vector<EditableModel>& models,
    std::vector<EditableLight>& lights,
    Renderer& renderer,
    EditorMaterialSelection& materialSelection)
{
    SetEditorPanelPosition(360.0f, 8.0f);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return false;
    }

    if (selection.type == EditorSelectionType::None)
    {
        ImGui::TextUnformatted("Select an object in the Scene or viewport.");
        ImGui::End();
        return false;
    }

    if (selection.type == EditorSelectionType::Model)
    {
        EditableModel* model = FindEditableModel(models, selection.modelId);
        if (model == nullptr)
        {
            selection.Clear();
            ImGui::TextUnformatted("Selected model is unavailable.");
            ImGui::End();
            return false;
        }
        ImGui::TextUnformatted(model->name.empty() ? "Model" : model->name.c_str());
        ImGui::SeparatorText("Transform");
        bool changed = false;
        changed |= ImGui::DragFloat3(
            "Position", &model->transform.position.x, 0.05f);
        changed |= ImGui::DragFloat3(
            "Rotation", &model->transform.rotationDegrees.x, 1.0f);
        changed |= ImGui::DragFloat3(
            "Scale", &model->transform.scale.x, 0.01f, 0.001f, 1000.0f);
        EditorValueConstraints::SanitizeScale(model->transform.scale);
        if (changed && model->IsValid())
            ApplyEditableModelTransform(*model, renderer);

        ImGui::SeparatorText("Materials");
        bool openMaterialEditor = false;
        bool hasAvailableMaterial = false;
        const std::vector<MaterialHandle> handles = model->GetUsedMaterials();
        for (std::size_t index = 0; index < handles.size(); ++index)
        {
            Renderer::MaterialSnapshot snapshot;
            if (!renderer.GetMaterialSnapshot(handles[index], snapshot))
                continue;
            hasAvailableMaterial = true;
            const std::string label = "Slot " + std::to_string(index + 1) +
                ": " + (snapshot.name.empty() ? "Material" : snapshot.name);
            ImGui::PushID(static_cast<int>(handles[index].id));
            if (ImGui::Selectable(label.c_str(),
                    !materialSelection.AllMaterials() &&
                    materialSelection.Owner() == model->id &&
                    materialSelection.Material().id == handles[index].id))
            {
                // 点击模型材质时回到模型范围，并打开共用的参数编辑面板。
                materialSelection.Select(model->id, handles[index]);
                openMaterialEditor = true;
            }
            ImGui::PopID();
            for (std::size_t sectionIndex = 0; sectionIndex < model->sections.size();
                 ++sectionIndex)
            {
                const EditableModelSection& section = model->sections[sectionIndex];
                if (section.material.id == handles[index].id)
                {
                    const std::string sectionName = section.name.empty()
                        ? "Section " + std::to_string(sectionIndex + 1) : section.name;
                    ImGui::TextDisabled("  %s", sectionName.c_str());
                }
            }
        }
        if (!hasAvailableMaterial)
            ImGui::TextUnformatted("No available materials used by this model.");
        ImGui::End();
        return openMaterialEditor;
    }

    EditableLight* light = FindEditableLight(lights, selection.lightId);
    if (light == nullptr)
    {
        selection.Clear();
        ImGui::TextUnformatted("Selected light is unavailable.");
        ImGui::End();
        return false;
    }

    ImGui::TextUnformatted(light->name.empty() ? "Light" : light->name.c_str());
    ImGui::TextDisabled("Type: %s", LightTypeName(light->proxy.type));
    ImGui::SeparatorText("Light Parameters");

    bool changed = false;
    if (light->proxy.type != LightType::Directional)
    {
        changed |= ImGui::DragFloat3(
            "Position", &light->transform.position.x, 0.05f);
    }
    else
    {
        changed |= ImGui::DragFloat3(
            "Direction", &light->proxy.direction.x, 0.01f, -1.0f, 1.0f);
        EditorValueConstraints::SanitizeDirection(light->proxy.direction);
    }

    changed |= ImGui::ColorEdit3("Color", &light->proxy.color.x);
    changed |= ImGui::DragFloat(
        "Intensity", &light->proxy.intensity, 0.05f, 0.0f, 100.0f);
    EditorValueConstraints::SanitizeColor(light->proxy.color);

    if (light->proxy.type != LightType::Directional)
    {
        changed |= ImGui::DragFloat(
            "Range", &light->proxy.range, 0.1f, 0.01f, 1000.0f);
    }
    EditorValueConstraints::SanitizeLightScalars(
        light->proxy.intensity, light->proxy.range);

    if (light->proxy.type == LightType::Spot)
    {
        float innerDegrees = light->proxy.innerConeAngle * 180.0f / Pi;
        float outerDegrees = light->proxy.outerConeAngle * 180.0f / Pi;
        changed |= ImGui::DragFloat(
            "Inner Cone", &innerDegrees, 0.5f, 0.0f, 89.0f, "%.1f deg");
        changed |= ImGui::DragFloat(
            "Outer Cone", &outerDegrees, 0.5f, 1.0f, 89.0f, "%.1f deg");
        EditorValueConstraints::SanitizeSpotConeDegrees(
            innerDegrees, outerDegrees);
        light->proxy.innerConeAngle = innerDegrees * Pi / 180.0f;
        light->proxy.outerConeAngle = outerDegrees * Pi / 180.0f;
        ImGui::TextDisabled("Spot direction targets the scene center.");
    }

    if (changed)
        ApplyEditableLightTransform(*light, renderer);

    ImGui::End();
    return false;
}
