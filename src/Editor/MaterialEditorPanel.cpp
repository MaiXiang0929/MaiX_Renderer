// SPDX-License-Identifier: MIT
#include "MaterialEditorPanel.h"

#include <array>
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
#include "Platform/Windows/FileDialog.h"
#include "Renderer/Core/Renderer.h"

namespace
{
const char* ShadingModelName(ShadingModel model)
{
    return model == ShadingModel::Toon ? "Toon" : "PBR";
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

void MaterialEditorPanel::Draw(Renderer& renderer, void* nativeWindowHandle)
{
    ImGui::SetNextWindowSize(ImVec2(520.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Material Editor"))
    {
        ImGui::End();
        return;
    }

    const std::vector<MaterialHandle> materialHandles =
        renderer.GetMaterialHandles();
    if (materialHandles.empty())
    {
        ImGui::TextUnformatted("No materials available.");
        ImGui::End();
        return;
    }

    if (m_SelectedMaterial >= materialHandles.size())
    {
        m_SelectedMaterial =
            static_cast<unsigned int>(materialHandles.size() - 1);
    }

    const MaterialHandle handle = materialHandles[m_SelectedMaterial];
    Renderer::MaterialSnapshot snapshot;
    if (!renderer.GetMaterialSnapshot(handle, snapshot))
    {
        ImGui::TextUnformatted("Selected material is unavailable.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginCombo("Material", snapshot.name.c_str()))
    {
        for (std::size_t index = 0; index < materialHandles.size(); ++index)
        {
            Renderer::MaterialSnapshot candidate;
            if (!renderer.GetMaterialSnapshot(materialHandles[index], candidate))
                continue;
            const bool selected = index == m_SelectedMaterial;
            ImGui::PushID(static_cast<int>(materialHandles[index].id));
            if (ImGui::Selectable(candidate.name.c_str(), selected))
                m_SelectedMaterial = static_cast<unsigned int>(index);
            if (selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
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
        changed |= ImGui::Checkbox(
            "Face Shadow", &properties.faceShadowEnabled);
        if (properties.faceShadowEnabled)
        {
            changed |= ImGui::InputFloat3(
                "Face Forward", &properties.faceForwardLocal.x, "%.3f");
            changed |= ImGui::InputFloat3(
                "Face Right", &properties.faceRightLocal.x, "%.3f");
            changed |= ImGui::SliderFloat(
                "Face Shadow Softness",
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
            changed |= ImGui::SliderFloat(
                "Outline Thickness",
                &properties.outlineThickness,
                MinimumOutlineThickness,
                MaximumOutlineThickness,
                "%.3f");
            changed |= ImGui::ColorEdit3(
                "Outline Color", &properties.outlineColor.x);
        }
    }

    int blendMode = snapshot.blendMode == BlendMode::AlphaBlend ? 1 : 0;
    if (ImGui::Combo("Blend Mode", &blendMode, "Opaque\0Alpha Blend\0"))
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
    if (ImGui::BeginTable(
            "MaterialTextures",
            4,
            ImGuiTableFlags_BordersInnerH |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableHeadersRow();

        for (std::size_t index = 0; index < names.size(); ++index)
        {
            const MaterialTextureSlot slot =
                static_cast<MaterialTextureSlot>(index);
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow(ImGuiTableRowFlags_None, 54.0f);
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(names[index]);

            ImGui::TableSetColumnIndex(1);
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

            ImGui::TableSetColumnIndex(2);
            const std::string displayName =
                TextureDisplayName(snapshot.textureSources[index]);
            ImGui::TextWrapped("%s", displayName.c_str());
            if (!snapshot.textureSources[index].empty() &&
                ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", snapshot.textureSources[index].c_str());
            }

            ImGui::TableSetColumnIndex(3);
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
        ImGui::EndTable();
    }

    if (!m_TextureMessage.empty())
    {
        const ImVec4 color = m_TextureMessageIsError
            ? ImVec4(1.0f, 0.35f, 0.30f, 1.0f)
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::TextColored(color, "%s", m_TextureMessage.c_str());
    }

    ImGui::End();
}
