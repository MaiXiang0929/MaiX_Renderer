// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/SceneLight.h"

#include <algorithm>


SceneLight* FindSceneLight(
    std::vector<SceneLight>& lights,
    LightId id)
{
    const auto iterator = std::find_if(
        lights.begin(), lights.end(),
        [id](const SceneLight& light) { return light.proxy.id == id; });
    return iterator == lights.end() ? nullptr : &*iterator;
}

const SceneLight* FindSceneLight(
    const std::vector<SceneLight>& lights,
    LightId id)
{
    const auto iterator = std::find_if(
        lights.begin(), lights.end(),
        [id](const SceneLight& light) { return light.proxy.id == id; });
    return iterator == lights.end() ? nullptr : &*iterator;
}
