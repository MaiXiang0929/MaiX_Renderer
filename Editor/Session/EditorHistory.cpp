// SPDX-License-Identifier: MIT
#include "Editor/Session/EditorHistory.h"

EditorHistory::State EditorHistory::Capture(const Actor& actor)
{
    return {actor.id, actor.parent, actor.local, actor.name, actor.visible};
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
    m_Pending = Action{kind,state,state,label};
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
            // Allocate before discarding redo so an allocation failure keeps the existing history.
            m_Actions.reserve(m_Actions.size()+1);
            m_Actions.erase(m_Actions.begin()+m_Cursor,m_Actions.end());
            m_Actions.push_back(std::move(*m_Pending));
            if (m_Actions.size() > 256) m_Actions.erase(m_Actions.begin());
            m_Cursor = m_Actions.size();
        }
    }
    m_Pending.reset();
}
void EditorHistory::Clear() { m_Pending.reset(); m_Actions.clear(); m_Cursor=0; }
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
    }
    error.clear(); return true;
}
bool EditorHistory::Undo(World& world, std::string& error)
{
    if (!CanUndo()) { error="Nothing to undo."; return false; }
    if (!Replay(world,m_Actions[m_Cursor-1],true,error)) return false;
    --m_Cursor; return true;
}
bool EditorHistory::Redo(World& world, std::string& error)
{
    if (!CanRedo()) { error="Nothing to redo."; return false; }
    if (!Replay(world,m_Actions[m_Cursor],false,error)) return false;
    ++m_Cursor; return true;
}
bool EditorHistory::Rename(World& world, ActorId id, const std::string& name, std::string& error)
{
    Finish(world);
    if (!Begin(world,id,EditorEditKind::Name,"Rename") || !world.Rename(id,name))
    { m_Pending.reset(); error="Name edit rejected."; return false; }
    Finish(world); error.clear(); return true;
}
bool EditorHistory::SetVisible(World& world, ActorId id, bool visible, std::string& error)
{
    Finish(world);
    if (!Begin(world,id,EditorEditKind::Visibility,"Visibility") || !world.SetVisible(id,visible))
    { m_Pending.reset(); error="Visibility edit rejected."; return false; }
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
    if (!world.Reparent(id,parent,true,error)) { m_Pending.reset(); return false; }
    Finish(world); return true;
}
