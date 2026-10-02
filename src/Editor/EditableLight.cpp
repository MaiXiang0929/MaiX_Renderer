// SPDX-License-Identifier: MIT
#include "EditableLight.h"

#include <algorithm>

#include "Renderer/Core/Renderer.h"

void ApplyEditableLightTransform(
    EditableLight& light,
    Renderer& renderer)
{
    light.proxy.position = light.transform.position;
    if (light.proxy.type == LightType::Directional)
    {
        Transform rigid = light.transform;
        rigid.position = cy::Vec3f(0.0f); rigid.scale = cy::Vec3f(1);
        const auto direction = rigid.ToMatrix() * cy::Vec4f(0,0,-1,0);
        light.proxy.direction = cy::Vec3f(direction.x,direction.y,direction.z).GetNormalized();
    }
    renderer.UpdateLight(light.proxy.id, light.proxy);
}

EditableLight* FindEditableLight(
    std::vector<EditableLight>& lights,
    LightId id)
{
    const auto iterator = std::find_if(
        lights.begin(), lights.end(),
        [id](const EditableLight& light) { return light.proxy.id == id; });
    return iterator == lights.end() ? nullptr : &*iterator;
}

const EditableLight* FindEditableLight(
    const std::vector<EditableLight>& lights,
    LightId id)
{
    const auto iterator = std::find_if(
        lights.begin(), lights.end(),
        [id](const EditableLight& light) { return light.proxy.id == id; });
    return iterator == lights.end() ? nullptr : &*iterator;
}
