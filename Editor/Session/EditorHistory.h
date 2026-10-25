// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/World.h"
#include <optional>
#include "Engine/Runtime/Engine/SceneSubtree.h"

enum class EditorEditKind { Transform, Name, Visibility, ParentLocal, Delete };

// Editor-owned CPU property/subtree history. Scene/Renderer are borrowed; Renderer owns GPU resources.
class EditorHistory
{
public:
    EditorHistory() = default;
    ~EditorHistory();
    EditorHistory(const EditorHistory&) = delete;
    EditorHistory& operator=(const EditorHistory&) = delete;
    // The scene and renderer are borrowed; clear before destroying either owner.
    void Bind(SceneState& scene, Renderer& renderer);
    bool Delete(ActorId actor, std::string& error);
    ActorId RestoredActor() const { return m_RestoredActor; }
    bool Begin(World& world, ActorId actor, EditorEditKind kind, const char* label);
    void Finish(World& world);
    void Clear();
    void DiscardRegion(const std::string& region);
    bool IsEditing() const { return m_Pending.has_value(); }
    bool CanUndo() const { return !IsEditing() && m_Cursor != 0; }
    bool CanRedo() const { return !IsEditing() && m_Cursor < m_Actions.size(); }
    const char* UndoLabel() const { return CanUndo() ? m_Actions[m_Cursor-1].label.c_str() : ""; }
    const char* RedoLabel() const { return CanRedo() ? m_Actions[m_Cursor].label.c_str() : ""; }
    bool Undo(World& world, std::string& error);
    bool Redo(World& world, std::string& error);
    bool Rename(World& world, ActorId actor, const std::string& name, std::string& error);
    bool SetVisible(World& world, ActorId actor, bool visible, std::string& error);
    bool Reparent(World& world, ActorId actor, ActorId parent, std::string& error);
private:
    struct State { ActorId id, parent; Transform local; std::string name; bool visible = true; std::string region; };
    struct Action { EditorEditKind kind; State before, after; std::string label; std::shared_ptr<SceneSubtreeArchive> subtree; bool pinned=false; };
    static State Capture(const Actor& actor);
    static bool Equal(EditorEditKind kind, const State& a, const State& b);
    bool Replay(World& world, const Action& action, bool undo, std::string& error);
    void Record(Action action);
    void Release(Action& action) noexcept;
    bool BoundTo(const World& world) const;
    std::vector<Action> m_Actions;
    std::size_t m_Cursor = 0;
    std::optional<Action> m_Pending;
    SceneState* m_Scene = nullptr;
    Renderer* m_Renderer = nullptr;
    std::weak_ptr<const int> m_RendererLifetime;
    ActorId m_RestoredActor;
};
