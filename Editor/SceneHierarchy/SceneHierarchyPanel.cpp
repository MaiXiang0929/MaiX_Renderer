// SPDX-License-Identifier: MIT
#include "Editor/SceneHierarchy/SceneHierarchyPanel.h"
#include "Editor/Session/EditorHistory.h"
#include "Editor/Session/EditorWidgets.h"
#include "Editor/Session/EditorTheme.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Runtime/Engine/WorldContentSession.h"
#include <algorithm>
#include <functional>
#include <imgui.h>

ActorId SceneHierarchyPanel::Draw(EditorSelection &selection, SceneState &scene, Renderer &renderer,
    EditorHistory* history, const std::function<void(const char*)>& invalidate,
    WorldContentSession* content,const std::function<void()>& drawContent)
{
    if (!ImGui::Begin("Scene"))
    {
        ImGui::End();
        return {};
    }
    ActorId removed, reparent, parent;
    if (drawContent) drawContent();
    bool detach = false;
    const float scale = EditorTheme::GetScale();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Scene Root");
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(),
        ImGui::GetWindowContentRegionMax().x - 28*scale));
    if (ImGui::Button("...",ImVec2(28*scale,0))) ImGui::OpenPopup("Scene Actions");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Create objects or manage the selection. Drag objects onto their parent.");
    if (ImGui::BeginPopup("Scene Actions"))
    {
        if (ImGui::MenuItem("Create Empty"))
        {
            if (history) history->Finish(scene.world);
            const auto id = scene.world.Create("Empty");
            if (id.IsValid())
            {
                if (invalidate) invalidate("Object created");
                else if (history) history->Clear();
                selection.SelectActor(id,scene);
            }
        }
        if (ImGui::BeginMenu("Create Light"))
        {
            const LightType types[] = {LightType::Directional, LightType::Point, LightType::Spot};
            const char *names[] = {"Directional", "Point", "Spot"};
            for (int i = 0; i < 3; ++i)
                if (ImGui::MenuItem(names[i]))
                {
                    Transform local;
                    local.position = scene.objCenter + cy::Vec3f(0, 3, 0);
                    local.rotationDegrees = {-90, 0, 0};
                    const auto id = CreateSceneLight(scene, renderer, types[i], local, m_Error);
                    if (id != InvalidLightId)
                    {
                        if (invalidate) invalidate("Light created");
                        else if (history) history->Clear();
                        selection.SelectLight(id);
                    }
                }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        const auto selected = selection.ResolveActor(scene);
        const auto* actor = scene.world.Find(selected);
        if (ImGui::MenuItem("Detach to Root",nullptr,false,actor && actor->parent.IsValid()))
        {
            reparent = selected;
            detach = true;
        }
        const auto label = "Delete Subtree (" + std::to_string(scene.world.Subtree(selected).size()) + ")";
        if (ImGui::MenuItem(label.c_str(),nullptr,false,actor != nullptr)) removed = selected;
        ImGui::EndPopup();
    }
    const auto selected = selection.ResolveActor(scene);
    std::string contentRegion;
    std::function<void(ActorId)> draw = [&](ActorId parentId) {
        for (const auto &a : scene.world.Actors())
        {
            if (a.parent != parentId)
                continue;
            if (content&&content->Ready()&&a.contentRegion!=contentRegion) continue;
            const bool hasChildren = std::any_of(scene.world.Actors().begin(), scene.world.Actors().end(),
                                                 [&](const Actor &child) { return child.parent == a.id; });
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_FramePadding;
            if (!hasChildren)
                flags |= ImGuiTreeNodeFlags_Leaf;
            if (a.id == selected)
                flags |= ImGuiTreeNodeFlags_Selected;
            const auto key = std::to_string(a.id.value);
            ImGui::PushID(key.c_str());
            const bool mainLight = a.kind == ActorKind::Light && a.componentId == scene.mainLightId;
            const bool visible = scene.world.IsVisible(a.id);
            const bool open = ImGui::TreeNodeEx("node", flags, "");
            const auto min = ImGui::GetItemRectMin();
            const auto max = ImGui::GetItemRectMax();
            const float iconX = min.x+ImGui::GetTreeNodeToLabelSpacing();
            const auto icon = a.kind == ActorKind::Model ? EditorWidgets::Icon::Mesh :
                a.kind == ActorKind::Light ? EditorWidgets::Icon::Light :
                a.kind == ActorKind::Camera ? EditorWidgets::Icon::Camera : EditorWidgets::Icon::World;
            auto* list = ImGui::GetWindowDrawList();
            list->PushClipRect(min,max,true);
            EditorWidgets::DrawIcon(icon,ImVec2(iconX,min.y+3*scale),16*scale);
            const auto label = a.name + (mainLight ? " (main light)" : "") + (visible ? "" : " (hidden)");
            list->AddText(ImVec2(iconX+22*scale,min.y+ImGui::GetStyle().FramePadding.y),
                ImGui::GetColorU32(visible ? ImGuiCol_Text : ImGuiCol_TextDisabled),label.c_str());
            list->PopClipRect();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",label.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                if (history) history->Finish(scene.world);
                selection.SelectActor(a.id, scene);
            }
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("MAIX_ACTOR", &a.id, sizeof(a.id));
                ImGui::TextUnformatted(a.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const auto *payload = ImGui::AcceptDragDropPayload("MAIX_ACTOR"))
                {
                    reparent = *static_cast<const ActorId *>(payload->Data);
                    parent = a.id;
                }
                ImGui::EndDragDropTarget();
            }
            if (open)
            {
                draw(a.id);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    };
    if (content&&content->Ready()) {
        contentRegion=content->Id();
        if (ImGui::TreeNodeEx("Persistent Content",ImGuiTreeNodeFlags_DefaultOpen)) { draw({}); ImGui::TreePop(); }
        for (const auto& region:content->Regions()) {
            ImGui::PushID(region.record.id.c_str()); contentRegion=region.record.id;
            const auto label=region.record.name+(region.loaded?"":" (unloaded)");
            if (ImGui::TreeNode(label.c_str())) {
                if (region.loaded) draw({});
                else for (const auto& descriptor:content->Catalog()) if (descriptor.region==region.record.id) {
                    ImGui::TextDisabled("%s",descriptor.name.c_str());
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Load the region to edit this object.");
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    } else draw({});
    if (reparent.IsValid())
    {
        if (history) history->Reparent(scene.world,reparent,detach ? ActorId{} : parent,m_Error);
        else scene.world.Reparent(reparent, detach ? ActorId{} : parent, true, m_Error);
    }
    if (!m_Error.empty())
        ImGui::TextWrapped("%s", m_Error.c_str());
    ImGui::End();
    return removed;
}
