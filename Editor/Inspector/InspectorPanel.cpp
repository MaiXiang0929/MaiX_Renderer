// SPDX-License-Identifier: MIT
#include "Editor/Inspector/InspectorPanel.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdio>
#include "Engine/Runtime/Engine/RenderSceneBridge.h"

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
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float right = viewport->WorkPos.x + viewport->WorkSize.x;
    const float x = right - width - offset;
    ImGui::SetNextWindowPos(
        ImVec2(x < viewport->WorkPos.x + 8.0f ? viewport->WorkPos.x + 8.0f : x, viewport->WorkPos.y + 16.0f),
        ImGuiCond_FirstUseEver);
}

const char *LightTypeName(LightType type)
{
    switch (type)
    {
    case LightType::Directional:
        return "Directional";
    case LightType::Spot:
        return "Spot";
    case LightType::Point:
        return "Point";
    }
    return "Unknown";
}
} // namespace

bool InspectorPanel::Draw(EditorSelection &selection, SceneState &scene, bool &cameraView, Renderer &renderer,
                          EditorMaterialSelection &materialSelection)
{
    auto &models = scene.models;
    auto &lights = scene.lights;
    auto &camera = scene.camera;
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

    const auto actorId = selection.ResolveActor(scene);
    const auto *actor = scene.world.Find(actorId);
    if (!actor)
    {
        selection.Clear();
        ImGui::TextUnformatted("Object unavailable.");
        ImGui::End();
        return false;
    }
    std::array<char, 256> name{};
    std::snprintf(name.data(), name.size(), "%s", actor->name.c_str());
    if (ImGui::InputText("Name", name.data(), name.size(), ImGuiInputTextFlags_EnterReturnsTrue))
        scene.world.Rename(actorId, name.data());
    actor = scene.world.Find(actorId);
    const auto parentId = actor->parent;
    const auto *parent = scene.world.Find(parentId);
    ActorId requestedParent = parentId;
    bool reparent = false;
    if (ImGui::BeginCombo("Parent", parent ? parent->name.c_str() : "Scene Root"))
    {
        if (ImGui::Selectable("Scene Root", !parentId.IsValid()))
        {
            requestedParent = {};
            reparent = true;
        }
        const auto descendants = scene.world.Subtree(actorId);
        for (const auto &candidate : scene.world.Actors())
        {
            if (std::find(descendants.begin(), descendants.end(), candidate.id) != descendants.end())
                continue;
            const auto id = std::to_string(candidate.id.value);
            ImGui::PushID(id.c_str());
            if (ImGui::Selectable(candidate.name.c_str(), candidate.id == parentId))
            {
                requestedParent = candidate.id;
                reparent = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (reparent)
        scene.world.Reparent(actorId, requestedParent, true, m_Error);
    actor = scene.world.Find(actorId);
    if (EditorWidgets::Section("Local Transform"))
    {
        auto local = actor->local;
        bool changed = EditorWidgets::DragVector3("Position (m)", &local.position.x, 0.005f);
        changed |= EditorWidgets::DragVector3("Rotation (deg)", &local.rotationDegrees.x, 0.5f);
        if (actor->kind == ActorKind::Empty || actor->kind == ActorKind::Model)
            changed |= EditorWidgets::DragVector3("Scale", &local.scale.x, 0.001f, 0.001f, 1000, "%.4f");
        if (changed)
            scene.world.SetLocal(actorId, local, m_Error);
    }
    if (!m_Error.empty())
        ImGui::TextWrapped("%s", m_Error.c_str());
    SynchronizeSceneActors(scene, renderer);
    if (selection.type == EditorSelectionType::Empty)
    {
        EditorWidgets::ObjectHeader(EditorWidgets::Icon::World, scene.world.Find(actorId)->name.c_str(),
                                    "Empty");
        ImGui::End();
        return false;
    }

    if (selection.type == EditorSelectionType::Model)
    {
        SceneModel *model = FindSceneModel(models, selection.modelId);
        if (model == nullptr)
        {
            selection.Clear();
            ImGui::TextUnformatted("Selected model is unavailable.");
            ImGui::End();
            return false;
        }
        EditorWidgets::ObjectHeader(EditorWidgets::Icon::Mesh,
                                    model->name.empty() ? "Model" : model->name.c_str(), "Mesh");
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
                const std::string label = "Slot " + std::to_string(index + 1) + ": " +
                                          (snapshot.name.empty() ? "Material" : snapshot.name);
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
                for (std::size_t sectionIndex = 0; sectionIndex < model->sections.size(); ++sectionIndex)
                {
                    const SceneModelSection &section = model->sections[sectionIndex];
                    if (section.material.id == handles[index].id)
                    {
                        const std::string sectionName = section.name.empty()
                                                            ? "Section " + std::to_string(sectionIndex + 1)
                                                            : section.name;
                        ImGui::TextDisabled("  %s", sectionName.c_str());
                    }
                }
            }
            if (!hasAvailableMaterial)
                ImGui::TextUnformatted("No available materials used by this model.");
        }
        if (model->importedSizeMeters && EditorWidgets::Section("Asset Info", false))
        {
            const auto &size = *model->importedSizeMeters;
            ImGui::TextWrapped("Imported X / Y / Z: %.4g / %.4g / %.4g m", size.x, size.y, size.z);
            if (model->sourceUnitMeters)
                ImGui::TextWrapped("Reported source unit: %.6g m/unit", *model->sourceUnitMeters);
            else
                ImGui::TextUnformatted("Reported source unit: unknown");
            ImGui::TextWrapped("Imported size excludes current root Transform.");
        }
        ImGui::End();
        return openMaterialEditor;
    }

    if (selection.IsCameraSelected(camera.id))
    {
        const auto previousCamera = camera;
        EditorWidgets::ObjectHeader(EditorWidgets::Icon::Camera, camera.name.c_str(), "Camera");
        ImGui::DragFloat("Vertical FOV", &camera.fovDegrees, 0.5f, 1, 150, "%.1f deg");
        ImGui::DragFloat("Near (m)", &camera.nearPlane, 0.01f, 0.0001f, 1000, "%.4f");
        ImGui::DragFloat("Far (m)", &camera.farPlane, 1, 0.001f, 100000);
        auto finiteVector = [](const cy::Vec3f &value) {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        };
        if (!finiteVector(camera.transform.position) || !finiteVector(camera.transform.rotationDegrees) ||
            !std::isfinite(camera.fovDegrees) || !std::isfinite(camera.nearPlane) ||
            !std::isfinite(camera.farPlane))
            camera = previousCamera;
        camera.fovDegrees = std::clamp(camera.fovDegrees, 1.0f, 150.0f);
        camera.nearPlane = std::clamp(camera.nearPlane, 0.0001f, 1000.0f);
        camera.farPlane = std::clamp(camera.farPlane, camera.nearPlane + 0.001f, 100000.0f);
        if (ImGui::Button(cameraView ? "Return to Editor View" : "View through Camera"))
            cameraView = !cameraView;
        ImGui::TextWrapped("Numpad 0 toggles camera view. Editor navigation keeps this camera fixed.");
        ImGui::End();
        return false;
    }

    SceneLight *light = FindSceneLight(lights, selection.lightId);
    if (light == nullptr)
    {
        selection.Clear();
        ImGui::TextUnformatted("Selected light is unavailable.");
        ImGui::End();
        return false;
    }

    EditorWidgets::ObjectHeader(EditorWidgets::Icon::Light,
                                light->name.empty() ? "Light" : light->name.c_str(),
                                LightTypeName(light->proxy.type));
    ImGui::SeparatorText("Light Parameters");

    bool changed = false;
    changed |= ImGui::ColorEdit3("Color", &light->proxy.color.x);
    changed |= ImGui::DragFloat("Intensity", &light->proxy.intensity, 0.05f, 0.0f, 100.0f);
    EditorValueConstraints::SanitizeColor(light->proxy.color);

    if (light->proxy.type != LightType::Directional)
    {
        changed |= ImGui::DragFloat("Range", &light->proxy.range, 0.1f, 0.01f, 1000.0f);
    }
    EditorValueConstraints::SanitizeLightScalars(light->proxy.intensity, light->proxy.range);

    if (light->proxy.type == LightType::Spot)
    {
        float innerDegrees = light->proxy.innerConeAngle * 180.0f / Pi;
        float outerDegrees = light->proxy.outerConeAngle * 180.0f / Pi;
        changed |= ImGui::DragFloat("Inner Cone", &innerDegrees, 0.5f, 0.0f, 89.0f, "%.1f deg");
        changed |= ImGui::DragFloat("Outer Cone", &outerDegrees, 0.5f, 1.0f, 89.0f, "%.1f deg");
        EditorValueConstraints::SanitizeSpotConeDegrees(innerDegrees, outerDegrees);
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
