// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/SceneLight.h"

#include <algorithm>

void UpdateSceneLightProxy(SceneLight& light)
{
    light.proxy.position=light.transform.position;
    if (light.proxy.type==LightType::Directional||light.proxy.type==LightType::Spot) {
        auto rigid=light.transform; rigid.position=cy::Vec3f(0.0f); rigid.scale=cy::Vec3f(1.0f);
        const auto direction=rigid.ToMatrix()*cy::Vec4f(0,0,-1,0);
        light.proxy.direction=cy::Vec3f(direction.x,direction.y,direction.z).GetNormalized();
    }
}


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
