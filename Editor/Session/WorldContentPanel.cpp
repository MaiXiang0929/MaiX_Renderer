// SPDX-License-Identifier: MIT
#include "Editor/Session/WorldContentPanel.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <imgui.h>
#include "Engine/Runtime/Engine/WorldStreaming.h"

WorldContentAction WorldContentPanel::Draw(WorldContentSession& content,SceneState& scene,const Renderer& renderer,
    const SceneAssetRoots& roots,const EditorSelection& selection,WorldStreaming* streaming)
{
    WorldContentAction action;
    if (!content.Ready()) return action;
    std::string error;
    if (!content.RefreshCatalog(scene,error)) ImGui::TextWrapped("%s",error.c_str());
    if (ImGui::CollapsingHeader("World Regions",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("%s",content.File().filename().u8string().c_str());
        if(streaming&&ImGui::TreeNode("Streaming")) {
            auto settings=streaming->Settings(); bool changed=ImGui::Checkbox("Automatic streaming",&settings.automatic);
            ImGui::SetNextItemWidth(110); changed|=ImGui::DragFloat("Load distance (m)",&settings.loadDistance,1,0,100000);
            ImGui::SetNextItemWidth(110); changed|=ImGui::DragFloat("Unload distance (m)",&settings.unloadDistance,1,0,100000);
            if(ImGui::TreeNode("Budgets")) {
                float time=static_cast<float>(settings.uploadMilliseconds);
                ImGui::SetNextItemWidth(110); if(ImGui::DragFloat("Upload/release (ms)",&time,0.1f,0.1f,100)) { settings.uploadMilliseconds=time; changed=true; }
                auto number=[&](const char* label,std::size_t& value,int divisor,int maximum) {
                    int displayed=static_cast<int>(value/divisor); ImGui::SetNextItemWidth(110);
                    if(ImGui::DragInt(label,&displayed,1,1,maximum)) { value=static_cast<std::size_t>(displayed)*divisor; changed=true; }
                };
                number("Operations/frame",settings.operationsPerFrame,1,1000);
                number("Upload MiB/frame",settings.uploadBytesPerFrame,1024*1024,1024);
                number("Queued requests",settings.maxRequests,1,1024);
                number("CPU candidates MiB",settings.cpuCandidateBytes,1024*1024,16384);
                number("Scene GPU MiB",settings.gpuSceneBytes,1024*1024,16384);
                number("CPU/region MiB",settings.preparation.cpuBytes,1024*1024,16384);
                number("Mesh/texture MiB",settings.preparation.resourceBytes,1024*1024,4096);
                number("Source file MiB",settings.preparation.sourceBytes,1024*1024,4096);
                ImGui::TextWrapped("Development limits. Time is a soft budget; a single GL operation or atomic activation may exceed it.");
                ImGui::TreePop();
            }
            if(changed&&!streaming->Configure(settings,error)) ImGui::TextWrapped("%s",error.c_str());
            const auto& stats=streaming->Statistics();
            ImGui::Text("Requests: %zu (peak %zu)",stats.queueDepth,stats.peakQueueDepth);
            ImGui::Text("CPU candidates/reserved: %.1f MiB",double(stats.cpuBytes)/(1024*1024));
            ImGui::Text("Scene GPU upper estimate: %.1f MiB",double(stats.gpuUpperEstimate)/(1024*1024));
            ImGui::Text("Scheduling: %.2f ms (peak %.2f)",stats.tickMilliseconds,stats.peakTickMilliseconds);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Scene GPU estimate includes history and candidates; excludes targets, environment and driver overhead. CPU excludes parser/decode temporary allocations.");
            ImGui::TreePop();
        }
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
            const auto* status=streaming?streaming->Status(region.record.id):nullptr;
            const bool busy=status&&(status->phase==WorldStreamPhase::Queued||status->phase==WorldStreamPhase::Preparing||status->phase==WorldStreamPhase::WaitingBudget||status->phase==WorldStreamPhase::Uploading||status->phase==WorldStreamPhase::Activating||status->phase==WorldStreamPhase::Cancelling);
            if(status) {
                if(busy) {
                    ImGui::TextUnformatted(WorldStreamPhaseName(status->phase));
                    if(status->total) ImGui::ProgressBar(float(status->completed)/float(status->total));
                }
                if(!status->message.empty()) ImGui::TextWrapped("%s",status->message.c_str());
                if(region.loaded) {
                    bool held=status->manuallyHeld;
                    if(ImGui::Checkbox("Keep loaded",&held)) action={held?WorldContentActionKind::Load:WorldContentActionKind::ReleaseHold,region.record.id};
                }
            }
            if(busy) {
                if(ImGui::SmallButton("Cancel loading")) action={WorldContentActionKind::CancelLoad,region.record.id};
            } else if(ImGui::SmallButton(region.loaded?"Unload":status&&status->phase==WorldStreamPhase::Failed?"Retry Load":"Load"))
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
