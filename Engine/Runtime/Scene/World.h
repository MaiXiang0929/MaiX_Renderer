// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Core/Math/Transform.h"
#include <cstdint>
#include <string>
#include <vector>

struct ActorId
{
    std::uint64_t value = 0;
    bool IsValid() const
    {
        return value != 0;
    }
    bool operator==(ActorId other) const
    {
        return value == other.value;
    }
    bool operator!=(ActorId other) const
    {
        return value != other.value;
    }
};
enum class ActorKind
{
    Empty,
    Model,
    Light,
    Camera
};
struct Actor
{
    ActorId id, parent;
    std::string name;
    Transform local, world;
    ActorKind kind = ActorKind::Empty;
    std::uint64_t componentId = 0;
    bool visible = true;
};

// CPU scene identity and transform owner. Component payloads live in SceneState;
// only the scene bridge may register/delete their actors with their render bindings.
class World
{
  public:
    class SubtreeSnapshot
    {
    public:
        struct Entry { std::size_t position; Actor actor; };
        const std::vector<Entry>& Entries() const { return m_Entries; }
        ActorId Root() const { return m_Root; }
    private:
        friend class World;
        std::uint64_t m_Owner = 0;
        ActorId m_Root;
        std::vector<Entry> m_Entries;
    };
    World() noexcept;
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    World(World &&other) noexcept;
    World &operator=(World &&other) noexcept;
    ActorId Create(std::string name, const Transform &local = {}, ActorKind kind = ActorKind::Empty,
                   std::uint64_t componentId = 0);
    const Actor *Find(ActorId id) const;
    const std::vector<Actor> &Actors() const
    {
        return m_Actors;
    }
    bool Rename(ActorId id, std::string name);
    bool SetVisible(ActorId id, bool visible);
    bool IsVisible(ActorId id) const;
    bool SetLocal(ActorId id, const Transform &value, std::string &error);
    bool SetWorld(ActorId id, const cy::Matrix4f &value, std::string &error);
    bool Reparent(ActorId id, ActorId parent, bool keepWorld, std::string &error);
    bool SetParentAndLocal(ActorId id, ActorId parent, const Transform& local, std::string& error);
    std::vector<ActorId> Subtree(ActorId id) const;
    bool Destroy(ActorId id);
    bool CaptureSubtree(ActorId id, SubtreeSnapshot& result, std::string& error) const;
    // Prepare without changing this World; committing the move preserves its identity domain.
    bool PrepareSubtree(const SubtreeSnapshot& snapshot, bool restore, World& result, std::string& error) const;

  private:
    std::vector<Actor> m_Actors;
    std::uint64_t m_Identity = 0;
    bool Commit(std::vector<Actor> &candidate, std::string &error);
};
