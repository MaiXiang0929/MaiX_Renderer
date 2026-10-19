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
    World() = default;
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    World(World &&) = default;
    World &operator=(World &&) = default;
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
    std::vector<ActorId> Subtree(ActorId id) const;
    bool Destroy(ActorId id);

  private:
    std::vector<Actor> m_Actors;
    bool Commit(std::vector<Actor> &candidate, std::string &error);
};
