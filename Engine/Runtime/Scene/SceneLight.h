// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "Engine/Core/Math/Transform.h"
#include "Engine/Runtime/Scene/World.h"
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"

struct SceneLight
{
    ActorId actor;
    std::string name;
    Transform transform;
    LightSceneProxy proxy;

    bool IsValid() const { return proxy.id != InvalidLightId; }
};

void ApplySceneLightTransform(
    SceneLight& light,
    class Renderer& renderer);

SceneLight* FindSceneLight(
    std::vector<SceneLight>& lights,
    LightId id);
const SceneLight* FindSceneLight(
    const std::vector<SceneLight>& lights,
    LightId id);
