// SPDX-License-Identifier: MIT
#include "Editor/Inspector/InspectorPanel.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include "Editor/Session/EditorWidgets.h"

#include "Engine/Runtime/Scene/SceneLight.h"
#include "Engine/Runtime/Scene/SceneModel.h"
#include "Editor/Session/EditorMaterialSelection.h"
#include "Editor/Session/EditorValueConstraints.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"

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
    std::vector<SceneModel>& models,
    std::vector<SceneLight>& lights,
        SceneCamera& camera, bool& cameraView,
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
        SceneModel* model = FindSceneModel(models, selection.modelId);
        if (model == nullptr)
        {
            selection.Clear();
            ImGui::TextUnformatted("Selected model is unavailable.");
            ImGui::End();
            return false;
        }
        EditorWidgets::ObjectHeader(EditorWidgets::Icon::Mesh,
            model->name.empty() ? "Model" : model->name.c_str(),"Mesh");
        bool changed = false;
        if (EditorWidgets::Section("Transform"))
        {
            changed |= EditorWidgets::DragVector3(
                "Position (m)", &model->transform.position.x, 0.005f);
            changed |= EditorWidgets::DragVector3(
                "Rotation", &model->transform.rotationDegrees.x, 1.0f);
            changed |= EditorWidgets::DragVector3(
                "Scale", &model->transform.scale.x, 0.001f, 0.001f, 1000.0f, "%.4f");
        }
        EditorValueConstraints::SanitizeScale(model->transform.scale);
        if (changed && model->IsValid())
            ApplySceneModelTransform(*model, renderer);

        bool openMaterialEditor = false;
        if (EditorWidgets::Section("Materials"))
        {
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
                    const SceneModelSection& section = model->sections[sectionIndex];
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
        }
        if (model->importedSizeMeters && EditorWidgets::Section("Asset Info",false))
        {
            const auto& size=*model->importedSizeMeters;
            ImGui::TextWrapped("Imported X / Y / Z: %.4g / %.4g / %.4g m",size.x,size.y,size.z);
            if (model->sourceUnitMeters)
                ImGui::TextWrapped("Reported source unit: %.6g m/unit",*model->sourceUnitMeters);
            else ImGui::TextUnformatted("Reported source unit: unknown");
            ImGui::TextWrapped("Imported size excludes current root Transform.");
        }
        ImGui::End();
        return openMaterialEditor;
    }

    if (selection.IsCameraSelected(camera.id))
    {
        const auto previousCamera = camera;
        EditorWidgets::ObjectHeader(EditorWidgets::Icon::Camera,camera.name.c_str(),"Camera");
        EditorWidgets::DragVector3("Position (m)", &camera.transform.position.x, 0.05f);
        EditorWidgets::DragVector3("Rotation", &camera.transform.rotationDegrees.x, 0.5f);
        ImGui::DragFloat("Vertical FOV", &camera.fovDegrees, 0.5f, 1, 150, "%.1f deg");
        ImGui::DragFloat("Near (m)", &camera.nearPlane, 0.01f, 0.0001f, 1000, "%.4f");
        ImGui::DragFloat("Far (m)", &camera.farPlane, 1, 0.001f, 100000);
        auto finiteVector = [](const cy::Vec3f& value) {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        };
        if (!finiteVector(camera.transform.position) || !finiteVector(camera.transform.rotationDegrees) ||
            !std::isfinite(camera.fovDegrees) || !std::isfinite(camera.nearPlane) || !std::isfinite(camera.farPlane))
            camera = previousCamera;
        camera.fovDegrees = std::clamp(camera.fovDegrees, 1.0f, 150.0f);
        camera.nearPlane = std::clamp(camera.nearPlane, 0.0001f, 1000.0f);
        camera.farPlane = std::clamp(camera.farPlane, camera.nearPlane + 0.001f, 100000.0f);
        if (ImGui::Button(cameraView ? "Return to Editor View" : "View through Camera"))
            cameraView = !cameraView;
        ImGui::TextWrapped("Numpad 0 toggles camera view. Editor navigation keeps this camera fixed.");
        ImGui::End(); return false;
    }

    SceneLight* light = FindSceneLight(lights, selection.lightId);
    if (light == nullptr)
    {
        selection.Clear();
        ImGui::TextUnformatted("Selected light is unavailable.");
        ImGui::End();
        return false;
    }

    EditorWidgets::ObjectHeader(EditorWidgets::Icon::Light,
        light->name.empty() ? "Light" : light->name.c_str(),LightTypeName(light->proxy.type));
    ImGui::SeparatorText("Light Parameters");

    bool changed = false;
    if (light->proxy.type != LightType::Directional)
    {
        changed |= EditorWidgets::DragVector3(
            "Position", &light->transform.position.x, 0.05f);
    }
    else
    {
        const auto previousRotation = light->transform.rotationDegrees;
        changed |= EditorWidgets::DragVector3(
            "Rotation", &light->transform.rotationDegrees.x, 0.5f);
        const auto rotation = light->transform.rotationDegrees;
        if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z))
            light->transform.rotationDegrees = previousRotation;
        ImGui::TextDisabled("Position does not affect directional lighting.");
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
        ImGui::TextDisabled("Cone follows the light direction.");
    }

    changed |= ImGui::Checkbox("Cast Shadow", &light->proxy.castsShadow);
    if (changed)
        ApplySceneLightTransform(*light, renderer);

    ImGui::End();
    return false;
}
