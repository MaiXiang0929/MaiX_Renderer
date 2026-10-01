// SPDX-License-Identifier: MIT
#include "MaterialEditorPanel.h"

#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "Assets/Import/ImageData.h"
#include "Assets/Import/ImageLoader.h"
#include "Editor/EditorMaterialSelection.h"
#include "Platform/Windows/FileDialog.h"
#include "Renderer/Core/Renderer.h"

namespace
{
void MaterialFieldLabel(const char* label)
{
    // 标签单独占一行，控件使用剩余宽度，避免窄 Inspector 列中长标签挤压输入框。
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

std::string TextureDisplayName(const std::string& source)
{
    if (source.empty())
        return "Not bound";
    if (source.rfind("Embedded: ", 0) == 0)
        return source;

    const std::filesystem::path path = std::filesystem::u8path(source);
    const std::string filename = path.filename().u8string();
    return filename.empty() ? source : filename;
}
}

void MaterialEditorPanel::Draw(
    Renderer& renderer, const EditorSelection& selection,
    const std::vector<EditableModel>& models,
    EditorMaterialSelection& materialSelection, void* nativeWindowHandle)
{
    ImGui::SetNextWindowSize(ImVec2(520.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Material Editor"))
    {
        ImGui::End();
        return;
    }

    bool allMaterials = materialSelection.AllMaterials();
    if (ImGui::Checkbox("All materials (debug)", &allMaterials))
        materialSelection.SetAllMaterials(allMaterials);
    const std::vector<MaterialHandle> materialHandles = materialSelection.Synchronize(
        selection, models, renderer.GetMaterialHandles());

    // 全局调试列表同样显示所属模型，避免不同 FBX 的同名材质难以区分。
    const auto materialLabel = [&models, &materialSelection](
        MaterialHandle material, const std::string& name)
    {
        std::string label = name.empty() ? "Material" : name;
        if (materialSelection.AllMaterials())
        {
            std::string owners;
            for (const EditableModel& model : models)
            {
                for (MaterialHandle used : model.GetUsedMaterials())
                {
                    if (used.id == material.id)
                    {
                        if (!owners.empty()) owners += ", ";
                        owners += model.name;
                        break;
                    }
                }
            }
            label = (owners.empty() ? "Scene" : owners) + " / " + label;
        }
        return label;
    };
    const EditableModel* owner = FindEditableModel(models, materialSelection.Owner());
    if (owner)
        ImGui::TextWrapped("Model: %s", owner->name.c_str());
    else if (allMaterials)
        ImGui::TextWrapped("Editing all scene materials (debug).");
    if (materialHandles.empty())
    {
        ImGui::TextWrapped("%s", allMaterials ? "No materials available." :
            "Select a model with available materials in the Scene or viewport.");
        m_LastMaterial = {};
        m_TextureMessage.clear();
        ImGui::End();
        return;
    }

    Renderer::MaterialSnapshot snapshot;
    const bool available = renderer.GetMaterialSnapshot(materialSelection.Material(), snapshot);
    const std::string preview = available
        ? materialLabel(snapshot.handle, snapshot.name) : "Select material...";
    MaterialFieldLabel("Material");
    if (ImGui::BeginCombo("##Material", preview.c_str()))
    {
        for (std::size_t index = 0; index < materialHandles.size(); ++index)
        {
            Renderer::MaterialSnapshot candidate;
            if (!renderer.GetMaterialSnapshot(materialHandles[index], candidate))
                continue;
            const bool selected = materialHandles[index].id == materialSelection.Material().id;
            ImGui::PushID(static_cast<int>(materialHandles[index].id));
            const std::string label = "Slot " + std::to_string(index + 1) +
                ": " + materialLabel(candidate.handle, candidate.name);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                if (allMaterials)
                    materialSelection.SelectGlobal(materialHandles[index]);
                else
                    materialSelection.Select(materialSelection.Owner(), materialHandles[index]);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    // 下拉框切换后立即重新取快照，参数和纹理操作始终作用于本帧选中的句柄。
    const MaterialHandle handle = materialSelection.Material();
    if (handle.id != m_LastMaterial.id)
    {
        m_LastMaterial = handle;
        m_TextureMessage.clear();
    }
    if (!renderer.GetMaterialSnapshot(handle, snapshot))
    {
        ImGui::TextWrapped("Selected material is unavailable. Choose a material above.");
        ImGui::End();
        return;
    }
    ImGui::PushID(static_cast<int>(handle.id));

    MaterialProperties properties = snapshot.properties;
    bool changed = false;
    int shadingModel = snapshot.properties.shadingModel == ShadingModel::Toon ? 1 : 0;
    MaterialFieldLabel("Shading Model");
    if (ImGui::Combo("##ShadingModel", &shadingModel, "PBR\0Toon\0"))
    {
        properties.shadingModel = shadingModel == 1 ? ShadingModel::Toon : ShadingModel::PBR;
        changed = true;
    }
    MaterialFieldLabel("Base Color");
    changed |= ImGui::ColorEdit3("##BaseColor", &properties.baseColor.x);
    MaterialFieldLabel("Metallic");
    changed |= ImGui::SliderFloat("##Metallic", &properties.metallic, 0.0f, 1.0f);
    MaterialFieldLabel("Roughness");
    changed |= ImGui::SliderFloat("##Roughness", &properties.roughness, 0.045f, 1.0f);
    MaterialFieldLabel("Ambient Occlusion");
    changed |= ImGui::SliderFloat("##AmbientOcclusion", &properties.ambientOcclusion, 0.0f, 1.0f);
    MaterialFieldLabel("Normal Scale");
    changed |= ImGui::SliderFloat("##NormalScale", &properties.normalScale, 0.0f, 4.0f);
    MaterialFieldLabel("Opacity");
    changed |= ImGui::SliderFloat("##Opacity", &properties.opacity, 0.0f, 1.0f);

    if (properties.shadingModel == ShadingModel::Toon)
    {
        MaterialFieldLabel("Toon Threshold");
        changed |= ImGui::SliderFloat("##ToonThreshold", &properties.toonThreshold, 0.0f, 1.0f);
        MaterialFieldLabel("Toon Shadow Strength");
        changed |= ImGui::SliderFloat("##ToonShadowStrength", &properties.toonShadowStrength, 0.0f, 1.0f);
        MaterialFieldLabel("Toon Shadow Color");
        changed |= ImGui::ColorEdit3("##ToonShadowColor", &properties.toonShadowColor.x);
        MaterialFieldLabel("Rim Light Strength");
        changed |= ImGui::SliderFloat("##RimLightStrength", &properties.rimLightStrength, 0.0f, 4.0f);
        MaterialFieldLabel("Rim Light Color");
        changed |= ImGui::ColorEdit3("##RimLightColor", &properties.rimLightColor.x);
        changed |= ImGui::Checkbox(
            "Face Shadow", &properties.faceShadowEnabled);
        if (properties.faceShadowEnabled)
        {
            MaterialFieldLabel("Face Forward");
            changed |= ImGui::InputFloat3(
                "##FaceForward", &properties.faceForwardLocal.x, "%.3f");
            MaterialFieldLabel("Face Right");
            changed |= ImGui::InputFloat3(
                "##FaceRight", &properties.faceRightLocal.x, "%.3f");
            MaterialFieldLabel("Face Shadow Softness");
            changed |= ImGui::SliderFloat(
                "##FaceShadowSoftness",
                &properties.faceShadowSoftness,
                MinimumFaceShadowSoftness,
                MaximumFaceShadowSoftness,
                "%.3f");
            changed |= ImGui::Checkbox(
                "Mirror Face Shadow X",
                &properties.faceShadowMirrorX);
        }
        changed |= ImGui::Checkbox("Outline", &properties.outlineEnabled);
        if (properties.outlineEnabled)
        {
            MaterialFieldLabel("Outline Thickness");
            changed |= ImGui::SliderFloat(
                "##OutlineThickness",
                &properties.outlineThickness,
                MinimumOutlineThickness,
                MaximumOutlineThickness,
                "%.3f");
            MaterialFieldLabel("Outline Color");
            changed |= ImGui::ColorEdit3(
                "##OutlineColor", &properties.outlineColor.x);
        }
    }

    int blendMode = snapshot.blendMode == BlendMode::AlphaBlend ? 1 : 0;
    MaterialFieldLabel("Blend Mode");
    if (ImGui::Combo("##BlendMode", &blendMode, "Opaque\0Alpha Blend\0"))
        changed = true;

    if (changed)
    {
        renderer.UpdateMaterial(
            handle,
            properties,
            blendMode == 1 ? BlendMode::AlphaBlend : BlendMode::Opaque);
    }

    ImGui::SeparatorText("Texture Slots");
    const std::array<const char*, MaterialTextureSlotCount> names = {
        "Base Color",
        "Normal",
        "ORM",
        "Displacement",
        "Legacy Specular",
        "Face Shadow"};
    // 每个槽位纵向排列预览、来源和操作，不要求窗口容纳四列固定宽度。
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        const MaterialTextureSlot slot =
            static_cast<MaterialTextureSlot>(index);
        ImGui::PushID(static_cast<int>(index));
        ImGui::SeparatorText(names[index]);

        if (snapshot.textureIds[index] != 0)
        {
            ImGui::Image(
                static_cast<ImTextureID>(snapshot.textureIds[index]),
                ImVec2(48.0f, 48.0f));
        }
        else
        {
            ImGui::Dummy(ImVec2(48.0f, 48.0f));
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        const std::string displayName =
            TextureDisplayName(snapshot.textureSources[index]);
        ImGui::TextWrapped("%s", displayName.c_str());
        if (!snapshot.textureSources[index].empty() &&
            ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", snapshot.textureSources[index].c_str());
        }
        ImGui::EndGroup();

        const float actionsWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Select..."))
        {
            const std::optional<std::filesystem::path> selected =
                FileDialog::OpenTextureFile(nativeWindowHandle);
            if (selected)
            {
                AssetImport::ImageData image;
                std::string error;
                if (!AssetImport::ImageLoader::LoadFromFile(
                        *selected, image, error))
                {
                    m_TextureMessage = std::move(error);
                    m_TextureMessageIsError = true;
                }
                else
                {
                    std::shared_ptr<Texture2D> texture =
                        Texture2D::CreateRGBA8(
                            image.width,
                            image.height,
                            image.pixels,
                            GetRequiredMaterialTextureColorSpace(slot));
                    if (!texture || !renderer.UpdateMaterialTexture(
                            handle,
                            slot,
                            std::move(texture),
                            selected->u8string()))
                    {
                        m_TextureMessage =
                            "Texture upload or material binding failed.";
                        m_TextureMessageIsError = true;
                    }
                    else
                    {
                        m_TextureMessage =
                            "Bound " + selected->filename().u8string() +
                            " to " + names[index] + ".";
                        m_TextureMessageIsError = false;
                    }
                }
            }
        }
        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonsWidth = ImGui::CalcTextSize("Select...").x +
            ImGui::CalcTextSize("Clear").x + 4.0f * style.FramePadding.x +
            style.ItemSpacing.x;
        if (actionsWidth >= buttonsWidth)
            ImGui::SameLine();
        ImGui::BeginDisabled(!snapshot.hasTextures[index]);
        if (ImGui::Button("Clear"))
        {
            if (renderer.ClearMaterialTexture(handle, slot))
            {
                m_TextureMessage = std::string("Cleared ") + names[index] + ".";
                m_TextureMessageIsError = false;
            }
            else
            {
                m_TextureMessage = "Unable to clear the texture slot.";
                m_TextureMessageIsError = true;
            }
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    if (!m_TextureMessage.empty())
    {
        const ImVec4 color = m_TextureMessageIsError
            ? ImVec4(1.0f, 0.35f, 0.30f, 1.0f)
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(color, "%s", m_TextureMessage.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::PopID();
    ImGui::End();
}
