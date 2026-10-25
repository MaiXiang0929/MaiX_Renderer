// SPDX-License-Identifier: MIT
#include "Editor/Session/WorldContentPanel.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <imgui.h>

WorldContentAction WorldContentPanel::Draw(WorldContentSession& content,SceneState& scene,const Renderer& renderer,
    const SceneAssetRoots& roots,const EditorSelection& selection)
{
    WorldContentAction action;
    if (!content.Ready()) return action;
    std::string error;
    if (!content.RefreshCatalog(scene,error)) ImGui::TextWrapped("%s",error.c_str());
    if (ImGui::CollapsingHeader("World Regions",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("%s",content.File().filename().u8string().c_str());
        if (ImGui::Selectable("Edit Persistent Content",scene.world.ContentRegion()==content.Id()))
            action={WorldContentActionKind::Edit,content.Id()};
        for (const auto& region:content.Regions()) {
            ImGui::PushID(region.record.id.c_str());
            const bool dirty=region.loaded&&content.IsDirty(region.record.id,scene,renderer,roots,error);
            const auto label=region.record.name+(region.loaded?" (loaded)":" (unloaded)")+(dirty?" *":"");
            if(ImGui::Selectable(label.c_str(),scene.world.ContentRegion()==region.record.id)&&region.loaded)
                action={WorldContentActionKind::Edit,region.record.id};
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Bounds (m): %.1f %.1f %.1f -> %.1f %.1f %.1f\n%s",
                region.record.minimum.x,region.record.minimum.y,region.record.minimum.z,
                region.record.maximum.x,region.record.maximum.y,region.record.maximum.z,region.record.source.path.c_str());
            if(ImGui::SmallButton(region.loaded?"Unload":"Load"))
                action={region.loaded?WorldContentActionKind::Unload:WorldContentActionKind::Load,region.record.id};
            if(region.loaded) {
                ImGui::SameLine();
                if(ImGui::SmallButton("Save + World")) action={WorldContentActionKind::Save,region.record.id};
                const auto pins=scene.world.ContentRegionPins(region.record.id);
                if(pins) {
                    ImGui::TextDisabled("Undo/Redo retains this region (%zu actions).",pins);
                    if(ImGui::SmallButton("Discard Region History...")) { m_ClearRegion=region.record.id; ImGui::OpenPopup("Discard Region History"); }
                }
                if(ImGui::BeginPopupModal("Discard Region History",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextUnformatted("Discard this region's Undo and Redo actions?\nCurrent content and other regions' history are kept.");
                    if(ImGui::Button("Discard History")) { action={WorldContentActionKind::DiscardHistory,m_ClearRegion}; ImGui::CloseCurrentPopup(); }
                    ImGui::SameLine(); if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }
        if(ImGui::TreeNode("Create Region")) {
            ImGui::InputText("Name",m_Name,sizeof(m_Name));
            ImGui::DragFloat3("Minimum (m)",&m_Minimum.x,1.0f);
            ImGui::DragFloat3("Maximum (m)",&m_Maximum.x,1.0f);
            if(ImGui::Button("Create Region + Save World")) {
                action.kind=WorldContentActionKind::CreateRegion; action.name=m_Name;
                action.minimum=m_Minimum; action.maximum=m_Maximum;
            }
            ImGui::TreePop();
        }
        const auto* model=FindSceneModel(scene.models,selection.modelId);
        if(model&&selection.type==EditorSelectionType::Model&&ImGui::TreeNode("Bind World Material")) {
            for(auto material:model->GetUsedMaterials()) {
                ImGui::PushID(static_cast<int>(material.id)); Renderer::MaterialSnapshot source;
                if(renderer.GetMaterialSnapshot(material,source)&&ImGui::BeginCombo(source.name.c_str(),"Choose shared material")) {
                    for(const auto& entry:scene.contentMaterials) if(entry.region==content.Id()) {
                        Renderer::MaterialSnapshot target;
                        if(renderer.GetMaterialSnapshot(entry.handle,target)&&ImGui::Selectable(target.name.c_str(),entry.handle.id==material.id)) {
                            action.kind=WorldContentActionKind::BindShared; action.model=model->id;
                            action.material=material; action.replacement=entry.handle;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
    }
    return action;
}
