// SPDX-License-Identifier: MIT
#include "Editor/Session/EditorHistory.h"
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"

EditorHistory::~EditorHistory() { Clear(); }
void EditorHistory::Bind(SceneState& scene, Renderer& renderer)
{
    if (m_Scene==&scene && m_Renderer==&renderer && !m_RendererLifetime.expired()) return;
    Clear(); m_Scene=&scene; m_Renderer=&renderer; m_RendererLifetime=renderer.Lifetime();
}
bool EditorHistory::BoundTo(const World& world) const
{
    return m_Scene && m_Renderer && !m_RendererLifetime.expired() && &m_Scene->world==&world;
}
void EditorHistory::Release(Action& action) noexcept
{
    if (action.pinned && m_Scene) { m_Scene->world.UnpinContentRegion(action.before.region); action.pinned=false; }
    if (action.subtree && m_Scene && m_Renderer && !m_RendererLifetime.expired())
        ReleaseSceneSubtree(*m_Scene,*m_Renderer,action.subtree);
    else action.subtree.reset();
}
void EditorHistory::Record(Action action)
{
    m_Actions.reserve(m_Actions.size()+1);
    for (std::size_t i=m_Cursor;i<m_Actions.size();++i) Release(m_Actions[i]);
    m_Actions.erase(m_Actions.begin()+m_Cursor,m_Actions.end());
    m_Actions.push_back(std::move(action));
    if (m_Actions.size()>256) { Release(m_Actions.front()); m_Actions.erase(m_Actions.begin()); }
    m_Cursor=m_Actions.size();
}
bool EditorHistory::Delete(ActorId actor, std::string& error)
{
    if (!m_Scene || !BoundTo(m_Scene->world)) { error="Delete history has no active scene."; return false; }
    Finish(m_Scene->world);
    const auto* target=m_Scene->world.Find(actor);
    if (!target) { error="Object is unavailable."; return false; }
    Action action{EditorEditKind::Delete,Capture(*target),Capture(*target),"Delete "+target->name,{}};
    // Reserve before detaching: history publication must not allocate after scene publication.
    m_Actions.reserve(m_Actions.size()+1);
    if (!action.before.region.empty()) { m_Scene->world.PinContentRegion(action.before.region); action.pinned=true; }
    try {
        if (!DetachSceneSubtree(*m_Scene,*m_Renderer,actor,action.subtree,error)) { Release(action); return false; }
    } catch (...) { Release(action); throw; }
    Record(std::move(action)); m_RestoredActor={}; return true;
}

EditorHistory::State EditorHistory::Capture(const Actor& actor)
{
    return {actor.id, actor.parent, actor.local, actor.name, actor.visible,actor.contentRegion};
}
bool EditorHistory::Equal(EditorEditKind kind, const State& a, const State& b)
{
    if (a.id != b.id) return false;
    if (kind == EditorEditKind::Name) return a.name == b.name;
    if (kind == EditorEditKind::Visibility) return a.visible == b.visible;
    if (a.parent != b.parent) return false;
    const auto same = [](const cy::Vec3f& x, const cy::Vec3f& y) {
        return x.x == y.x && x.y == y.y && x.z == y.z;
    };
    return same(a.local.position,b.local.position) && same(a.local.rotationDegrees,b.local.rotationDegrees) &&
        same(a.local.scale,b.local.scale);
}
bool EditorHistory::Begin(World& world, ActorId id, EditorEditKind kind, const char* label)
{
    if (m_Pending && m_Pending->before.id == id && m_Pending->kind == kind && m_Pending->label == label)
        return true;
    Finish(world);
    const auto* actor = world.Find(id);
    if (!actor) return false;
    const auto state = Capture(*actor);
    Action action{kind,state,state,label};
    if (BoundTo(world)&&!state.region.empty()) { world.PinContentRegion(state.region); action.pinned=true; }
    m_Pending = std::move(action);
    return true;
}
void EditorHistory::Finish(World& world)
{
    if (!m_Pending) return;
    if (const auto* actor = world.Find(m_Pending->before.id))
    {
        m_Pending->after = Capture(*actor);
        if (!Equal(m_Pending->kind,m_Pending->before,m_Pending->after))
        {
            Record(std::move(*m_Pending));
        }
        else Release(*m_Pending);
    }
    else Release(*m_Pending);
    m_Pending.reset();
}
void EditorHistory::Clear()
{
    if (m_Pending) Release(*m_Pending);
    m_Pending.reset();
    for (auto& action:m_Actions) Release(action);
    m_Actions.clear(); m_Cursor=0; m_RestoredActor={};
}
void EditorHistory::DiscardRegion(const std::string& region)
{
    if (m_Pending) {
        if (m_Pending->before.region!=region) return;
        Release(*m_Pending); m_Pending.reset();
    }
    std::vector<Action> kept; kept.reserve(m_Actions.size());
    std::size_t cursor=0;
    for (std::size_t i=0;i<m_Actions.size();++i) {
        auto& action=m_Actions[i];
        if (action.before.region==region) Release(action);
        else { if (i<m_Cursor) ++cursor; kept.push_back(std::move(action)); }
    }
    m_Actions.swap(kept); m_Cursor=cursor; m_RestoredActor={};
}
bool EditorHistory::Replay(World& world, const Action& action, bool undo, std::string& error)
{
    const auto& expected = undo ? action.after : action.before;
    const auto& value = undo ? action.before : action.after;
    const auto* actor = world.Find(value.id);
    if (!actor || !Equal(action.kind,Capture(*actor),expected))
    { error="History target is unavailable or has changed outside this action."; return false; }
    switch(action.kind)
    {
    case EditorEditKind::Transform: return world.SetLocal(value.id,value.local,error);
    case EditorEditKind::ParentLocal: return world.SetParentAndLocal(value.id,value.parent,value.local,error);
    case EditorEditKind::Name: if (!world.Rename(value.id,value.name)) return false; break;
    case EditorEditKind::Visibility: if (!world.SetVisible(value.id,value.visible)) return false; break;
    case EditorEditKind::Delete: error="Delete requires a bound scene."; return false;
    }
    error.clear(); return true;
}
bool EditorHistory::Undo(World& world, std::string& error)
{
    m_RestoredActor={};
    if (!CanUndo()) { error="Nothing to undo."; return false; }
    auto& action=m_Actions[m_Cursor-1];
    if (action.kind==EditorEditKind::Delete)
    {
        if (!BoundTo(world) || !action.subtree) { error="Delete history scene is unavailable."; return false; }
        if (!RestoreSceneSubtree(*m_Scene,*m_Renderer,*action.subtree,error)) return false;
        m_RestoredActor=action.before.id;
    }
    else if (!Replay(world,action,true,error)) return false;
    --m_Cursor; return true;
}
bool EditorHistory::Redo(World& world, std::string& error)
{
    m_RestoredActor={};
    if (!CanRedo()) { error="Nothing to redo."; return false; }
    auto& action=m_Actions[m_Cursor];
    if (action.kind==EditorEditKind::Delete)
    {
        if (!BoundTo(world)) { error="Delete history scene is unavailable."; return false; }
        std::shared_ptr<SceneSubtreeArchive> refreshed;
        // Capture current untracked material edits, instead of restoring obsolete bindings later.
        if (!DetachSceneSubtree(*m_Scene,*m_Renderer,action.before.id,refreshed,error)) return false;
        // Refresh the resource archive without releasing this action's residency pin.
        ReleaseSceneSubtree(*m_Scene,*m_Renderer,action.subtree); action.subtree=std::move(refreshed);
    }
    else if (!Replay(world,action,false,error)) return false;
    ++m_Cursor; return true;
}
bool EditorHistory::Rename(World& world, ActorId id, const std::string& name, std::string& error)
{
    Finish(world);
    if (!Begin(world,id,EditorEditKind::Name,"Rename") || !world.Rename(id,name))
    { if (m_Pending) Release(*m_Pending); m_Pending.reset(); error="Name edit rejected."; return false; }
    Finish(world); error.clear(); return true;
}
bool EditorHistory::SetVisible(World& world, ActorId id, bool visible, std::string& error)
{
    Finish(world);
    if (!Begin(world,id,EditorEditKind::Visibility,"Visibility") || !world.SetVisible(id,visible))
    { if (m_Pending) Release(*m_Pending); m_Pending.reset(); error="Visibility edit rejected."; return false; }
    Finish(world); error.clear(); return true;
}
bool EditorHistory::Reparent(World& world, ActorId id, ActorId parent, std::string& error)
{
    Finish(world);
    // Preserve exact authored TRS on an unchanged parent.
    const auto* current = world.Find(id);
    if (current && current->parent == parent) { error.clear(); return true; }
    if (!Begin(world,id,EditorEditKind::ParentLocal,"Reparent"))
    { error="Object unavailable."; return false; }
    if (!world.Reparent(id,parent,true,error)) { Release(*m_Pending); m_Pending.reset(); return false; }
    Finish(world); return true;
}
