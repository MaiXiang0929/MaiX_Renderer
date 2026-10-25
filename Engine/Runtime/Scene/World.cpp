// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/World.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <limits>
#include <utility>

namespace
{
std::atomic<std::uint64_t> nextActorId{1};
std::atomic<std::uint64_t> nextWorldId{1};
}
World::World() noexcept : m_Identity(nextWorldId.fetch_add(1))
{
    if (!m_Identity || m_Identity==std::numeric_limits<std::uint64_t>::max()) std::terminate();
}
World::World(World&& other) noexcept
    : m_Actors(std::move(other.m_Actors)),m_Identity(std::exchange(other.m_Identity,0)) {}
World& World::operator=(World&& other) noexcept
{
    if (this!=&other) { m_Actors=std::move(other.m_Actors); m_Identity=std::exchange(other.m_Identity,0); }
    return *this;
}
const Actor *World::Find(ActorId id) const
{
    const auto it =
        std::find_if(m_Actors.begin(), m_Actors.end(), [&](const Actor &a) { return a.id == id; });
    return it == m_Actors.end() ? nullptr : &*it;
}
ActorId World::Create(std::string name, const Transform &local, ActorKind kind, std::uint64_t componentId)
{
    Transform check;
    if (!TryDecomposeTransform(local.ToMatrix(), check))
        return {};
    const auto key = nextActorId.fetch_add(1);
    if (!key || key == std::numeric_limits<std::uint64_t>::max())
        std::terminate();
    Actor actor;
    actor.id = {key};
    actor.name = name.empty() ? "Object" : std::move(name);
    actor.local = local;
    actor.world = local;
    actor.kind = kind;
    actor.componentId = componentId;
    m_Actors.push_back(std::move(actor));
    return ActorId{key};
}
bool World::Rename(ActorId id, std::string name)
{
    if (!Find(id) || name.empty())
        return false;
    for (auto &a : m_Actors)
        if (a.id == id)
            a.name = std::move(name);
    return true;
}
bool World::SetVisible(ActorId id, bool visible)
{
    for (auto &actor : m_Actors)
        if (actor.id == id)
        {
            actor.visible = visible;
            return true;
        }
    return false;
}
bool World::IsVisible(ActorId id) const
{
    const auto *actor = Find(id);
    if (!actor)
        return false;
    for (; actor; actor = Find(actor->parent))
        if (!actor->visible)
            return false;
    return true;
}
bool World::Commit(std::vector<Actor> &candidate, std::string &error)
{
    std::vector<unsigned char> state(candidate.size());
    std::function<bool(std::size_t)> update = [&](std::size_t i) {
        if (state[i] == 2)
            return true;
        if (state[i] == 1)
            return false;
        state[i] = 1;
        auto matrix = candidate[i].local.ToMatrix();
        if (candidate[i].parent.IsValid())
        {
            auto p = std::find_if(candidate.begin(), candidate.end(),
                                  [&](const Actor &a) { return a.id == candidate[i].parent; });
            if (p == candidate.end() || !update(static_cast<std::size_t>(p - candidate.begin())))
                return false;
            matrix = p->world.ToMatrix() * matrix;
        }
        if (!TryDecomposeTransform(matrix, candidate[i].world))
            return false;
        state[i] = 2;
        return true;
    };
    for (std::size_t i = 0; i < candidate.size(); ++i)
        if (!update(i))
        {
            error = "Transform rejected: hierarchy must be acyclic, finite, invertible and free of shear.";
            return false;
        }
    m_Actors.swap(candidate);
    error.clear();
    return true;
}
bool World::SetLocal(ActorId id, const Transform &value, std::string &error)
{
    if (!Find(id))
    {
        error = "Object is unavailable.";
        return false;
    }
    auto candidate = m_Actors;
    for (auto &a : candidate)
        if (a.id == id)
            a.local = value;
    return Commit(candidate, error);
}
bool World::SetWorld(ActorId id, const cy::Matrix4f &value, std::string &error)
{
    const auto *actor = Find(id);
    if (!actor)
    {
        error = "Object is unavailable.";
        return false;
    }
    auto local = value;
    if (const auto *p = Find(actor->parent))
        local = p->world.ToMatrix().GetInverse() * value;
    Transform decomposed;
    if (!TryDecomposeTransform(local, decomposed))
    {
        error = "World edit rejected: local transform would contain shear or be singular.";
        return false;
    }
    return SetLocal(id, decomposed, error);
}
bool World::Reparent(ActorId id, ActorId parent, bool keepWorld, std::string &error)
{
    const auto *actor = Find(id);
    if (!actor || (parent.IsValid() && !Find(parent)))
    {
        error = "Object or parent is unavailable.";
        return false;
    }
    for (auto p = Find(parent); p; p = Find(p->parent))
        if (p->id == id)
        {
            error = "An object cannot be parented to itself or its descendants.";
            return false;
        }
    auto candidate = m_Actors;
    for (auto &a : candidate)
        if (a.id == id)
        {
            a.parent = parent;
            if (keepWorld)
            {
                auto local = actor->world.ToMatrix();
                if (const auto *p = Find(parent))
                    local = p->world.ToMatrix().GetInverse() * local;
                if (!TryDecomposeTransform(local, a.local))
                {
                    error = "Reparent rejected: preserving world transform would require shear.";
                    return false;
                }
            }
        }
    return Commit(candidate, error);
}
std::vector<ActorId> World::Subtree(ActorId id) const
{
    std::vector<ActorId> result;
    if (!Find(id))
        return result;
    result.push_back(id);
    for (std::size_t i = 0; i < result.size(); ++i)
        for (const auto &a : m_Actors)
            if (a.parent == result[i])
                result.push_back(a.id);
    return result;
}
bool World::SetParentAndLocal(ActorId id, ActorId parent, const Transform& local, std::string& error)
{
    if (!Find(id) || (parent.IsValid() && !Find(parent)))
    { error="Object or parent is unavailable."; return false; }
    auto candidate = m_Actors;
    for (auto& actor : candidate)
        if (actor.id == id) { actor.parent=parent; actor.local=local; }
    return Commit(candidate,error);
}
bool World::Destroy(ActorId id)
{
    const auto removed = Subtree(id);
    if (removed.empty())
        return false;
    m_Actors.erase(std::remove_if(m_Actors.begin(), m_Actors.end(),
                                  [&](const Actor &a) {
                                      return std::find(removed.begin(), removed.end(), a.id) != removed.end();
                                  }),
                   m_Actors.end());
    return true;
}
bool World::CaptureSubtree(ActorId id, SubtreeSnapshot& result, std::string& error) const
{
    const auto ids=Subtree(id);
    if (ids.empty()) { error="Subtree is unavailable."; return false; }
    SubtreeSnapshot snapshot;
    snapshot.m_Owner=m_Identity; snapshot.m_Root=id;
    for (std::size_t i=0;i<m_Actors.size();++i)
        if (std::find(ids.begin(),ids.end(),m_Actors[i].id)!=ids.end())
            snapshot.m_Entries.push_back({i,m_Actors[i]});
    result=std::move(snapshot); error.clear(); return true;
}
bool World::PrepareSubtree(const SubtreeSnapshot& snapshot, bool restore, World& result, std::string& error) const
{
    if (&result==this || !m_Identity || snapshot.m_Owner!=m_Identity || snapshot.m_Entries.empty())
    { error="Subtree belongs to a different world or is unavailable."; return false; }
    auto candidate=m_Actors;
    if (restore)
    {
        for (const auto& entry:snapshot.m_Entries)
        {
            if (Find(entry.actor.id)) { error="Subtree identity is already active."; return false; }
            candidate.insert(candidate.begin()+std::min(entry.position,candidate.size()),entry.actor);
        }
    }
    else
    {
        const auto ids=Subtree(snapshot.m_Root);
        if (ids.size()!=snapshot.m_Entries.size()) { error="Subtree membership changed."; return false; }
        for (const auto& entry:snapshot.m_Entries)
        {
            const auto* actor=Find(entry.actor.id);
            if (!actor || std::find(ids.begin(),ids.end(),actor->id)==ids.end())
            { error="Subtree identity changed."; return false; }
        }
        candidate.erase(std::remove_if(candidate.begin(),candidate.end(),[&](const Actor& actor) {
            return std::find(ids.begin(),ids.end(),actor.id)!=ids.end();
        }),candidate.end());
    }
    World prepared;
    prepared.m_Identity=m_Identity;
    if (!prepared.Commit(candidate,error)) return false;
    result=std::move(prepared); return true;
}
