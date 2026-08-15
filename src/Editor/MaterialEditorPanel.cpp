// SPDX-License-Identifier: MIT
#include "MaterialEditorPanel.h"

#include <array>
#include <cstdio>

#include <imgui.h>

#include "Renderer/Core/Renderer.h"

namespace
{
const char* ShadingModelName(ShadingModel model)
{
    return model == ShadingModel::Toon ? "Toon" : "PBR";
}
}

void MaterialEditorPanel::Draw(Renderer& renderer)
{
    ImGui::SetNextWindowSize(ImVec2(380.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Material Editor"))
    {
        ImGui::End();
        return;
    }

    const std::size_t materialCount = renderer.GetMaterialResourceCount();
    if (materialCount == 0)
    {
        ImGui::TextUnformatted("No materials available.");
        ImGui::End();
        return;
    }

    if (m_SelectedMaterial >= materialCount)
        m_SelectedMaterial = static_cast<unsigned int>(materialCount - 1);

    std::array<char, 32> label{};
    std::snprintf(label.data(), label.size(), "Material %u", m_SelectedMaterial);
    if (ImGui::BeginCombo("Material", label.data()))
    {
        for (std::size_t index = 0; index < materialCount; ++index)
        {
            std::snprintf(label.data(), label.size(), "Material %zu", index);
            const bool selected = index == m_SelectedMaterial;
            if (ImGui::Selectable(label.data(), selected))
                m_SelectedMaterial = static_cast<unsigned int>(index);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const MaterialHandle handle = renderer.GetMaterialHandle(m_SelectedMaterial);
    Renderer::MaterialSnapshot snapshot;
    if (!renderer.GetMaterialSnapshot(handle, snapshot))
    {
        ImGui::TextUnformatted("Selected material is unavailable.");
        ImGui::End();
        return;
    }

    MaterialProperties properties = snapshot.properties;
    bool changed = false;
    int shadingModel = snapshot.properties.shadingModel == ShadingModel::Toon ? 1 : 0;
    if (ImGui::Combo("Shading Model", &shadingModel, "PBR\0Toon\0"))
    {
        properties.shadingModel = shadingModel == 1 ? ShadingModel::Toon : ShadingModel::PBR;
        changed = true;
    }
    changed |= ImGui::ColorEdit3("Base Color", &properties.baseColor.x);
    changed |= ImGui::SliderFloat("Metallic", &properties.metallic, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Roughness", &properties.roughness, 0.045f, 1.0f);
    changed |= ImGui::SliderFloat("Ambient Occlusion", &properties.ambientOcclusion, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Normal Scale", &properties.normalScale, 0.0f, 4.0f);
    changed |= ImGui::SliderFloat("Opacity", &properties.opacity, 0.0f, 1.0f);

    if (properties.shadingModel == ShadingModel::Toon)
    {
        changed |= ImGui::SliderFloat("Toon Threshold", &properties.toonThreshold, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Toon Shadow Strength", &properties.toonShadowStrength, 0.0f, 1.0f);
        changed |= ImGui::ColorEdit3("Toon Shadow Color", &properties.toonShadowColor.x);
        changed |= ImGui::SliderFloat("Rim Light Strength", &properties.rimLightStrength, 0.0f, 4.0f);
        changed |= ImGui::ColorEdit3("Rim Light Color", &properties.rimLightColor.x);
    }

    int blendMode = snapshot.blendMode == BlendMode::AlphaBlend ? 1 : 0;
    if (ImGui::Combo("Blend Mode", &blendMode, "Opaque\0Alpha Blend\0"))
        changed = true;

    ImGui::SeparatorText("Texture Slots");
    const std::array<const char*, MaterialTextureSlotCount> names = {
        "Base Color", "Normal", "ORM", "Displacement", "Legacy Specular"};
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        ImGui::BulletText("%s: %s", names[index],
            snapshot.hasTextures[index] ? "bound" : "not bound");
    }

    if (changed)
    {
        renderer.UpdateMaterial(
            handle,
            properties,
            blendMode == 1 ? BlendMode::AlphaBlend : BlendMode::Opaque);
    }

    ImGui::End();
}
