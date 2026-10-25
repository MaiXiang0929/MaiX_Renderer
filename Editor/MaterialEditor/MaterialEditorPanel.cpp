// SPDX-License-Identifier: MIT
#include "Editor/MaterialEditor/MaterialEditorPanel.h"
#include "Editor/Session/EditorTheme.h"
#include "Engine/Core/Math/ColorSpace.h"

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

#include "Engine/Asset/Importer/ImageData.h"
#include "Engine/Asset/Importer/ImageLoader.h"
#include "Editor/Session/EditorMaterialSelection.h"
#include "Engine/Core/Platform/Windows/FileDialog.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Rendering/RHI/OpenGL/OpenGLNativeBridge.h"

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

void DrawTextureSourceImage(const char* role, const AssetImport::TextureSourceImage& image)
{
    if (image.embeddedId)
        ImGui::TextWrapped("%s: %s (embedded %llu)", role,
            image.path.u8string().c_str(), static_cast<unsigned long long>(image.embeddedId));
    else
        ImGui::TextWrapped("%s: %s", role, image.path.u8string().c_str());
}

void DrawTextureReconstruction(const AssetImport::TextureSource& source)
{
    if (source.kind == AssetImport::TextureSourceKind::Orm)
    {
        ImGui::TextWrapped("Generated ORM: %u x %u", source.width, source.height);
        const std::array<const char*, 3> roles = {"AO -> R", "Roughness -> G", "Metallic -> B"};
        for (std::size_t index = 0; index < roles.size(); ++index)
        {
            if (!source.ormInputs[index])
            {
                ImGui::TextWrapped("%s: default 1", roles[index]);
                continue;
            }
            const auto& input = *source.ormInputs[index];
            DrawTextureSourceImage(roles[index], input.image);
            ImGui::Text("Source channel %c", input.channel < 4 ? "RGBA"[input.channel] : '?');
        }
    }
    else
    {
        DrawTextureSourceImage("Source", source.image);
        if (source.kind == AssetImport::TextureSourceKind::Normal)
            ImGui::TextWrapped("%s", source.flipGreen
                ? "Normal Y: DirectX (-Y) -> OpenGL (+Y); Green flipped"
                : "Normal Y: OpenGL (+Y); Green unchanged");
    }
}
}

void MaterialEditorPanel::Draw(
    Renderer& renderer, const EditorSelection& selection,
    const std::vector<SceneModel>& models,
    EditorMaterialSelection& materialSelection, MaterialEditRequest& request,
    void* nativeWindowHandle, const std::vector<MaterialHandle>* availableMaterials)
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
        selection, models, availableMaterials ? *availableMaterials : renderer.GetMaterialHandles());

    // 全局调试列表同样显示所属模型，避免不同 FBX 的同名材质难以区分。
    const auto materialLabel = [&models, &materialSelection](
        MaterialHandle material, const std::string& name)
    {
        std::string label = name.empty() ? "Material" : name;
        if (materialSelection.AllMaterials())
        {
            std::string owners;
            for (const SceneModel& model : models)
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
    const SceneModel* owner = FindSceneModel(models, materialSelection.Owner());
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
    const MaterialHandle selectedHandle = materialSelection.Material();
    MaterialHandle handle = selectedHandle;
    if (handle.id != m_LastMaterial.id)
    {
        m_LastMaterial = handle;
        m_EditParent = false;
        m_TextureMessage.clear();
    }
    if (!renderer.GetMaterialSnapshot(handle, snapshot))
    {
        ImGui::TextWrapped("Selected material is unavailable. Choose a material above.");
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(snapshot.isInstance ? "Material Instance" : "Material");
    if (snapshot.isInstance) ImGui::TextWrapped("Parent: %s", snapshot.parentName.c_str());
    ImGui::BeginDisabled(!owner || allMaterials);
    if (!snapshot.isInstance && ImGui::Button("Create Instance"))
        request = {MaterialEditAction::CreateInstance, owner ? owner->id : ModelId{}, selectedHandle};
    if (snapshot.isInstance && ImGui::Button("Use Parent Material"))
        request = {MaterialEditAction::UseParent, owner ? owner->id : ModelId{}, selectedHandle};
    ImGui::EndDisabled();
    if (request.action != MaterialEditAction::None) { ImGui::End(); return; }
    if (snapshot.isInstance)
    {
        ImGui::Checkbox("Edit Parent", &m_EditParent);
        if (m_EditParent)
        {
            handle = snapshot.parent;
            renderer.GetMaterialSnapshot(handle, snapshot);
            ImGui::TextWrapped("Editing parent material: changes affect its inheriting instances.");
        }
    }
    ImGui::PushID(static_cast<int>(handle.id));
    if (snapshot.isInstance && ImGui::Button("Reset All Overrides"))
        renderer.ResetMaterialOverrides(handle);
    renderer.GetMaterialSnapshot(handle, snapshot);
    MaterialProperties properties = snapshot.properties;
    bool changed = false;

    // UI 显式设置单项覆盖，不能用浮点差值推断继承状态。
    const auto editField = [&](MaterialParameter parameter, const char* label, const auto& draw)
    {
        ImGui::PushID(static_cast<int>(parameter));
        bool enabled = snapshot.overrides.test(static_cast<std::size_t>(parameter));
        if (snapshot.isInstance)
        {
            if (ImGui::Checkbox("Override", &enabled))
            {
                renderer.UpdateMaterialOverride(handle, parameter, properties, enabled);
                renderer.GetMaterialSnapshot(handle, snapshot);
                properties = snapshot.properties;
            }
            ImGui::SameLine();
            ImGui::TextDisabled(enabled ? "Local" : "Inherited");
        }
        ImGui::BeginDisabled(snapshot.isInstance && !enabled);
        MaterialFieldLabel(label);
        if (draw())
        {
            if (snapshot.isInstance) renderer.UpdateMaterialOverride(handle, parameter, properties);
            else changed = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    };
    int shadingModel = static_cast<int>(properties.shadingModel);
    ImGui::BeginDisabled(snapshot.isInstance);
    MaterialFieldLabel(snapshot.isInstance ? "Shading Model (parent)" : "Shading Model");
    if (ImGui::Combo("##ShadingModel", &shadingModel, "PBR\0Toon\0Unlit\0"))
    {
        properties.shadingModel = static_cast<ShadingModel>(shadingModel);
        changed = true;
    }
    ImGui::EndDisabled();
    int surfaceSide = static_cast<int>(properties.surfaceSide);
    ImGui::BeginDisabled(snapshot.isInstance);
    MaterialFieldLabel(snapshot.isInstance ? "Surface Side (parent)" : "Surface Side");
    if (ImGui::Combo("##SurfaceSide", &surfaceSide, "Front\0Back\0Double\0Legacy\0"))
    {
        properties.surfaceSide = static_cast<SurfaceSide>(surfaceSide);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Front/Back select the rendered side. Double renders both sides with corrected back-face normals.\nLegacy preserves old scene culling and normals. Instances inherit their parent's setting.");
    if (properties.surfaceSide == SurfaceSide::Legacy)
        ImGui::TextDisabled("Legacy: old scene culling and normals.");
    editField(MaterialParameter::BaseColor, "Base Color (sRGB)", [&] {
        auto display = LinearToSrgb(properties.baseColor);
        if (!ImGui::ColorEdit3("##Value", &display.x)) return false;
        properties.baseColor = SrgbToLinear(display);
        return true;
    });
    editField(MaterialParameter::SpecularColor, "Dielectric F0 Multiplier", [&] { return ImGui::ColorEdit3("##Value", &properties.specularColor.x); });
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Linear multiplier of the non-metal F0 (0.04 at white). Metallic F0 uses Base Color.\nRetains the old Legacy Specular Color values; this is not UE's Specular mapping or IOR.");
    editField(MaterialParameter::Shininess, "Legacy Shininess", [&] { return ImGui::SliderFloat("##Value", &properties.shininess, 1.000f, 256.000f); });
    editField(MaterialParameter::EnvironmentReflectivity, "Environment Reflectivity", [&] { return ImGui::SliderFloat("##Value", &properties.environmentReflectivity, 0.000f, 1.000f); });
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Artistic multiplier of IBL specular only. 1 is unscaled; it does not change direct highlights or F0.");
    editField(MaterialParameter::Metallic, "Metallic", [&] { return ImGui::SliderFloat("##Value", &properties.metallic, 0.000f, 1.000f); });
    editField(MaterialParameter::Roughness, "Roughness", [&] { return ImGui::SliderFloat("##Value", &properties.roughness, 0.000f, 1.000f); });
    editField(MaterialParameter::AmbientOcclusion, "Ambient Occlusion", [&] { return ImGui::SliderFloat("##Value", &properties.ambientOcclusion, 0.000f, 1.000f); });
    editField(MaterialParameter::NormalScale, "Normal Scale", [&] { return ImGui::SliderFloat("##Value", &properties.normalScale, 0.000f, 4.000f); });
    editField(MaterialParameter::Opacity, "Opacity", [&] { return ImGui::SliderFloat("##Value", &properties.opacity, 0.000f, 1.000f); });
    editField(MaterialParameter::EmissiveColor, "Emissive Color (sRGB)", [&] {
        auto display = LinearToSrgb(properties.emissiveColor);
        if (!ImGui::ColorEdit3("##Value", &display.x)) return false;
        properties.emissiveColor = SrgbToLinear(display);
        return true;
    });
    editField(MaterialParameter::EmissiveIntensity, "Emissive Intensity", [&] {
        return ImGui::DragFloat("##Value", &properties.emissiveIntensity, 0.1f, 0.0f, MaximumEmissiveIntensity, "%.3f");
    });
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("HDR multiplier of Emissive Color and its optional sRGB texture. 0 disables emission.\nEmission participates in exposure and Bloom; it does not illuminate nearby objects.");
    if (properties.shadingModel == ShadingModel::Unlit)
        ImGui::TextWrapped("Unlit outputs emission only. Base Color alpha and Opacity still control coverage; object shadow casting is independent.");

    if (snapshot.blendMode == BlendMode::Masked)
        editField(MaterialParameter::AlphaCutoff, "Alpha Cutoff", [&] { return ImGui::SliderFloat("##Value", &properties.alphaCutoff, 0.0f, 1.0f); });

    if (properties.shadingModel == ShadingModel::Toon)
    {
        editField(MaterialParameter::ToonThreshold, "Toon Threshold", [&] { return ImGui::SliderFloat("##Value", &properties.toonThreshold, 0.000f, 1.000f); });
        editField(MaterialParameter::ToonShadowStrength, "Toon Shadow Strength", [&] { return ImGui::SliderFloat("##Value", &properties.toonShadowStrength, 0.000f, 1.000f); });
        editField(MaterialParameter::ToonShadowColor, "Toon Shadow Color", [&] { return ImGui::ColorEdit3("##Value", &properties.toonShadowColor.x); });
        editField(MaterialParameter::RimLightStrength, "Rim Light Strength", [&] { return ImGui::SliderFloat("##Value", &properties.rimLightStrength, 0.000f, 4.000f); });
        editField(MaterialParameter::RimLightColor, "Rim Light Color", [&] { return ImGui::ColorEdit3("##Value", &properties.rimLightColor.x); });
        editField(MaterialParameter::FaceShadowEnabled, "Face Shadow", [&] { return ImGui::Checkbox("##Value", &properties.faceShadowEnabled); });
        editField(MaterialParameter::FaceFrame, "Face Forward / Right", [&] {
            bool changed = ImGui::InputFloat3("##Forward", &properties.faceForwardLocal.x, "%.3f");
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed |= ImGui::InputFloat3("##Right", &properties.faceRightLocal.x, "%.3f");
            return changed;
        });
        editField(MaterialParameter::FaceShadowSoftness, "Face Shadow Softness", [&] { return ImGui::SliderFloat("##Value", &properties.faceShadowSoftness, 0.000f, 0.250f); });
        editField(MaterialParameter::FaceShadowMirrorX, "Mirror Face Shadow X", [&] { return ImGui::Checkbox("##Value", &properties.faceShadowMirrorX); });
        const bool outlineSupported = properties.surfaceSide == SurfaceSide::Front ||
            properties.surfaceSide == SurfaceSide::Legacy;
        if (!outlineSupported)
            ImGui::TextWrapped("Inverted-hull outline is unavailable for Back and Double surfaces.");
        ImGui::BeginDisabled(!outlineSupported);
        editField(MaterialParameter::OutlineEnabled, "Outline", [&] { return ImGui::Checkbox("##Value", &properties.outlineEnabled); });
        editField(MaterialParameter::OutlineThickness, "Outline Thickness", [&] { return ImGui::SliderFloat("##Value", &properties.outlineThickness, 0.000f, 0.200f, "%.5f", ImGuiSliderFlags_Logarithmic); });
        editField(MaterialParameter::OutlineColor, "Outline Color", [&] { return ImGui::ColorEdit3("##Value", &properties.outlineColor.x); });
        ImGui::EndDisabled();

    }
    int blendMode = static_cast<int>(snapshot.blendMode);
    ImGui::BeginDisabled(snapshot.isInstance);
    MaterialFieldLabel(snapshot.isInstance ? "Blend Mode (parent)" : "Blend Mode");
    if (ImGui::Combo("##BlendMode", &blendMode, "Opaque\0Alpha Blend\0Masked\0")) changed = true;
    ImGui::EndDisabled();
    if (changed)
        renderer.UpdateMaterial(handle, properties,
            static_cast<BlendMode>(blendMode));
    renderer.GetMaterialSnapshot(handle, snapshot);

    ImGui::SeparatorText("Texture Slots");
    const std::array<const char*, MaterialTextureSlotCount> names = {
        "Base Color",
        "Normal",
        "ORM",
        "Displacement",
        "Legacy Specular",
        "Face Shadow",
        "Emissive"};
    // 每个槽位纵向排列预览、来源和操作，不要求窗口容纳四列固定宽度。
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        const MaterialTextureSlot slot =
            static_cast<MaterialTextureSlot>(index);
        ImGui::PushID(static_cast<int>(index));
        ImGui::SeparatorText(names[index]);
        ImGui::Text("Sampling: %s", GetRequiredMaterialTextureColorSpace(slot) ==
            TextureColorSpace::SRGB ? "sRGB" : "Linear");
        if (snapshot.isInstance)
        {
            const auto mode = snapshot.textureModes[index];
            ImGui::TextDisabled(mode == TextureOverrideMode::Inherit ? "Inherited" :
                mode == TextureOverrideMode::Replace ? "Replaced" : "Disabled");
            if (ImGui::Button("Reset to Parent"))
                renderer.ResetMaterialTexture(handle, slot);
        }

        if (snapshot.textureIds[index] != 0)
        {
            ImGui::Image(
                static_cast<ImTextureID>(GetOpenGLTexture(renderer.Device(),snapshot.textureIds[index])),
                ImVec2(48.0f, 48.0f));
        }
        else
        {
            ImGui::Dummy(ImVec2(48.0f, 48.0f));
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        const auto& recipe = snapshot.textureRecipes[index];
        const std::string displayName = recipe && recipe->kind == AssetImport::TextureSourceKind::Orm
            ? "Generated ORM" : TextureDisplayName(snapshot.textureSources[index]);
        ImGui::TextWrapped("%s", displayName.c_str());
        if ((!recipe || recipe->kind != AssetImport::TextureSourceKind::Orm) &&
            !snapshot.textureSources[index].empty() &&
            ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", snapshot.textureSources[index].c_str());
        }
        ImGui::EndGroup();
        if (recipe)
            DrawTextureReconstruction(*recipe);

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
                        Texture2D::CreateRGBA8(renderer.Device(),
                            image.width,
                            image.height,
                            image.pixels,
                            GetRequiredMaterialTextureColorSpace(slot));
                    if (texture) texture->SetSource(std::filesystem::absolute(*selected));
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
        ImGui::BeginDisabled(!snapshot.hasTextures[index] && !snapshot.isInstance);
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
            ? EditorTheme::GetPalette().error
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(color, "%s", m_TextureMessage.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::PopID();
    ImGui::End();
}
