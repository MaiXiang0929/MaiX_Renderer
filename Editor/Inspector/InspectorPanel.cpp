// SPDX-License-Identifier: MIT
#include "Editor/Inspector/InspectorPanel.h"
#include "Editor/Session/EditorHistory.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdio>
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Core/Math/ColorSpace.h"

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
                          EditorMaterialSelection &materialSelection, EditorHistory* history)
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
    auto icon = EditorWidgets::Icon::World;
    const char* type = "Empty";
    if (actor->kind == ActorKind::Model) { icon = EditorWidgets::Icon::Mesh; type = "Mesh"; }
    else if (actor->kind == ActorKind::Camera) { icon = EditorWidgets::Icon::Camera; type = "Camera"; }
    else if (actor->kind == ActorKind::Light)
    {
        icon = EditorWidgets::Icon::Light;
        const auto* light = FindSceneLight(lights, selection.lightId);
        type = light ? LightTypeName(light->proxy.type) : "Light";
    }
    bool visible = actor->visible;
    if (EditorWidgets::ObjectHeader(icon,actor->name.c_str(),type,&visible))
    {
        if (history) history->SetVisible(scene.world,actorId,visible,m_Error);
        else scene.world.SetVisible(actorId, visible);
    }
    if (visible && !scene.world.IsVisible(actorId))
        ImGui::TextDisabled("Hidden by parent");
    std::array<char, 256> name{};
    std::snprintf(name.data(), name.size(), "%s", actor->name.c_str());
    const bool properties = EditorWidgets::BeginProperties("##Identity");
    if (properties) EditorWidgets::PropertyLabel("Name");
    if (properties && ImGui::InputText("##Name", name.data(), name.size(), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (history) history->Rename(scene.world,actorId,name.data(),m_Error);
        else scene.world.Rename(actorId, name.data());
    }
    actor = scene.world.Find(actorId);
    const auto parentId = actor->parent;
    const auto *parent = scene.world.Find(parentId);
    ActorId requestedParent = parentId;
    bool reparent = false;
    if (properties) EditorWidgets::PropertyLabel("Parent");
    if (properties && ImGui::BeginCombo("##Parent", parent ? parent->name.c_str() : "Scene Root"))
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
    if (properties) ImGui::EndTable();
    if (reparent)
    {
        if (history) history->Reparent(scene.world,actorId,requestedParent,m_Error);
        else scene.world.Reparent(actorId, requestedParent, true, m_Error);
    }
    actor = scene.world.Find(actorId);
    if (EditorWidgets::Section("Local Transform"))
    {
        const auto kind = actor->kind;
        auto edit = [&](const char* label, int field, float speed, float minimum, float maximum, const char* format) {
            auto local = scene.world.Find(actorId)->local;
            auto* values = field == 0 ? &local.position.x : field == 1 ? &local.rotationDegrees.x : &local.scale.x;
            EditorWidgets::EditLifecycle lifecycle;
            const bool changed = EditorWidgets::DragVector3(label,values,speed,minimum,maximum,format,&lifecycle);
            if (history && lifecycle.activated) history->Finish(scene.world);
            if (history && (lifecycle.activated || changed))
                history->Begin(scene.world,actorId,EditorEditKind::Transform,label);
            if (changed) scene.world.SetLocal(actorId,local,m_Error);
            if (history && lifecycle.deactivated && !lifecycle.active) history->Finish(scene.world);
        };
        edit("Position (m)",0,0.005f,0,0,"%.3f");
        edit("Rotation (deg)",1,0.5f,0,0,"%.3f");
        if (kind == ActorKind::Empty || kind == ActorKind::Model)
            edit("Scale",2,0.001f,0.001f,1000,"%.4f");
    }
    if (!m_Error.empty())
        ImGui::TextWrapped("%s", m_Error.c_str());
    SynchronizeSceneActors(scene, renderer);
    if (selection.type == EditorSelectionType::Empty)
    {
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
                if (EditorWidgets::IconSelectable("##Material", EditorWidgets::Icon::Mesh, label.c_str(),
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
        if (EditorWidgets::Section("Camera") && EditorWidgets::BeginProperties("##Camera"))
        {
            EditorWidgets::PropertyLabel("Vertical FOV");
            ImGui::DragFloat("##FOV", &camera.fovDegrees, 0.5f, 1, 150, "%.1f deg");
            EditorWidgets::PropertyLabel("Near (m)");
            ImGui::DragFloat("##Near", &camera.nearPlane, 0.01f, 0.0001f, 1000, "%.4f");
            EditorWidgets::PropertyLabel("Far (m)");
            ImGui::DragFloat("##Far", &camera.farPlane, 1, 0.001f, 100000);
            ImGui::EndTable();
        }
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

    if (!EditorWidgets::Section("Light Parameters"))
    {
        ImGui::End();
        return false;
    }

    bool changed = false;
    const bool lightProperties = EditorWidgets::BeginProperties("##Light");
    if (lightProperties) EditorWidgets::PropertyLabel("Type");
    if (lightProperties && ImGui::BeginCombo("##Type", LightTypeName(light->proxy.type)))
    {
        for (const auto type : {LightType::Directional, LightType::Point, LightType::Spot})
            if (ImGui::Selectable(LightTypeName(type), light->proxy.type == type))
            {
                light->proxy.type = type;
                changed = true;
            }
        ImGui::EndCombo();
    }
    if (lightProperties) ImGui::EndTable();
    if (light->proxy.id == scene.mainLightId)
        ImGui::TextUnformatted("Main Light");
    else if (ImGui::Button("Set as Main Light"))
        scene.mainLightId = light->proxy.id;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Main light has shadow priority and drives the default NPR key light.");
    const bool lightValues = EditorWidgets::BeginProperties("##LightValues");
    if (lightValues)
    {
        EditorWidgets::PropertyLabel("Color (sRGB)");
        auto display = LinearToSrgb(light->proxy.color);
        if (ImGui::ColorEdit3("##Color", &display.x))
        {
            light->proxy.color = SrgbToLinear(display);
            changed = true;
        }
        EditorWidgets::PropertyLabel("Intensity Model");
        int falloff = static_cast<int>(light->proxy.falloff);
        if (ImGui::Combo("##Falloff", &falloff, "Legacy\0Physical\0"))
        {
            light->proxy.falloff = static_cast<LightFalloff>(falloff);
            changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Physical: local lights use inverse-square falloff in meters and candela; directional lights use lux.\nLegacy retains range-only attenuation and unitless intensity. Switching keeps the numeric intensity; retune it explicitly.");
        const bool physical = light->proxy.falloff == LightFalloff::InverseSquare;
        EditorWidgets::PropertyLabel(!physical ? "Intensity (unitless)" :
            light->proxy.type == LightType::Directional ? "Intensity (lux)" : "Intensity (cd)");
        changed |= ImGui::DragFloat("##Intensity", &light->proxy.intensity, physical ? 1.0f : 0.05f,
            0.0f, physical ? MaximumPhysicalLightIntensity : 100.0f, "%.3f");
    }
    EditorValueConstraints::SanitizeColor(light->proxy.color);

    if (lightValues && light->proxy.type != LightType::Directional)
    {
        EditorWidgets::PropertyLabel("Range (m)");
        changed |= ImGui::DragFloat("##Range", &light->proxy.range, 0.1f, 0.01f, 1000.0f);
    }
    EditorValueConstraints::SanitizeLightScalars(light->proxy.intensity, light->proxy.range);
    if (light->proxy.falloff == LightFalloff::InverseSquare)
    {
        light->proxy.intensity = std::min(light->proxy.intensity, MaximumPhysicalLightIntensity);
        light->proxy.range = std::min(light->proxy.range, MaximumPhysicalLightRange);
    }

    if (lightValues && light->proxy.type == LightType::Spot)
    {
        float innerDegrees = light->proxy.innerConeAngle * 180.0f / Pi;
        float outerDegrees = light->proxy.outerConeAngle * 180.0f / Pi;
        EditorWidgets::PropertyLabel("Inner Cone");
        changed |= ImGui::DragFloat("##InnerCone", &innerDegrees, 0.5f, 0.0f, 89.0f, "%.1f deg");
        EditorWidgets::PropertyLabel("Outer Cone");
        changed |= ImGui::DragFloat("##OuterCone", &outerDegrees, 0.5f, 1.0f, 89.0f, "%.1f deg");
        EditorValueConstraints::SanitizeSpotConeDegrees(innerDegrees, outerDegrees);
        light->proxy.innerConeAngle = innerDegrees * Pi / 180.0f;
        light->proxy.outerConeAngle = outerDegrees * Pi / 180.0f;
    }
    if (lightValues) ImGui::EndTable();
    if (light->proxy.type == LightType::Spot)
        ImGui::TextWrapped("Cone half-angles; local -Z aims the light.");

    changed |= ImGui::Checkbox("Cast Shadow", &light->proxy.castsShadow);
    if (light->proxy.castsShadow)
    {
        ImGui::BeginDisabled(!scene.shadowSettings.multiLight);
        changed |= ImGui::SliderFloat("Shadow bias (texels)", &light->proxy.shadowBias, 0, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderFloat("Normal bias (texels)", &light->proxy.shadowNormalBias, 0, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Slope-dependent receiver offset, measured in shadow texels.");
        if (light->proxy.type == LightType::Directional)
        {
            int cascades = light->proxy.cascadeCount == 1 ? 0 : light->proxy.cascadeCount == 2 ? 1 : 2;
            if (ImGui::Combo("Cascades", &cascades, "1 (scene fit)\0 2\0 4\0"))
            { light->proxy.cascadeCount = cascades == 0 ? 1 : cascades == 1 ? 2 : 4; changed = true; }
            changed |= ImGui::DragFloat("Shadow distance", &light->proxy.shadowDistance, 0.5f, 0.2f, 100000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderFloat("Split lambda", &light->proxy.splitLambda, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0: uniform split distances; 1: logarithmic splits favoring nearby detail.");
        }
        ImGui::EndDisabled();
        if (!scene.shadowSettings.multiLight)
            ImGui::TextWrapped("Legacy scope uses original bias and a single map. Enable Multiple shadow lights in Render Settings to edit shadow quality.");
    }
    if (changed)
    {
        // Transform and visibility edits above may not have reached PrepareFrame yet.
        light->transform = scene.world.Find(light->actor)->world;
        light->proxy.visible = scene.world.IsVisible(light->actor);
        ApplySceneLightTransform(*light, renderer);
    }

    ImGui::End();
    return false;
}
