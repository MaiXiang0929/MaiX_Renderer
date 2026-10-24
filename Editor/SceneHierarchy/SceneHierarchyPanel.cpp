// SPDX-License-Identifier: MIT
#include "Editor/SceneHierarchy/SceneHierarchyPanel.h"
#include "Editor/Session/EditorWidgets.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include <algorithm>
#include <functional>
#include <imgui.h>

ActorId SceneHierarchyPanel::Draw(EditorSelection &selection, SceneState &scene, Renderer &renderer)
{
    if (!ImGui::Begin("Scene"))
    {
        ImGui::End();
        return {};
    }
    ActorId removed, reparent, parent;
    bool detach = false;
    if (ImGui::Button("Create Empty"))
        selection.SelectActor(scene.world.Create("Empty"), scene);
    ImGui::SameLine();
    if (ImGui::Button("Create Light"))
        ImGui::OpenPopup("Create Light Menu");
    if (ImGui::BeginPopup("Create Light Menu"))
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
                    selection.SelectLight(id);
            }
        ImGui::EndPopup();
    }
    const auto selected = selection.ResolveActor(scene);
    std::function<void(ActorId)> draw = [&](ActorId parentId) {
        for (const auto &a : scene.world.Actors())
        {
            if (a.parent != parentId)
                continue;
            const bool hasChildren = std::any_of(scene.world.Actors().begin(), scene.world.Actors().end(),
                                                 [&](const Actor &child) { return child.parent == a.id; });
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!hasChildren)
                flags |= ImGuiTreeNodeFlags_Leaf;
            if (a.id == selected)
                flags |= ImGuiTreeNodeFlags_Selected;
            const auto key = std::to_string(a.id.value);
            ImGui::PushID(key.c_str());
            const bool mainLight = a.kind == ActorKind::Light && a.componentId == scene.mainLightId;
            const bool open = ImGui::TreeNodeEx("node", flags, "%s%s%s", a.name.c_str(),
                mainLight ? " (main light)" : "",
                scene.world.IsVisible(a.id)?"":" (hidden)");
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                selection.SelectActor(a.id, scene);
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
    draw({});
    ImGui::Separator();
    ImGui::TextDisabled("Drag an object onto its parent.");
    ImGui::BeginDisabled(!selected.IsValid());
    if (ImGui::Button("Detach to Root"))
    {
        reparent = selected;
        detach = true;
    }
    const auto count = scene.world.Subtree(selected).size();
    const auto label = "Delete Subtree (" + std::to_string(count) + ")";
    if (ImGui::Button(label.c_str()))
        removed = selected;
    ImGui::EndDisabled();
    if (reparent.IsValid())
        scene.world.Reparent(reparent, detach ? ActorId{} : parent, true, m_Error);
    if (!m_Error.empty())
        ImGui::TextWrapped("%s", m_Error.c_str());
    ImGui::End();
    return removed;
}
