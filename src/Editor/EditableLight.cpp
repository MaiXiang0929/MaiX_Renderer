// SPDX-License-Identifier: MIT
#include "EditableLight.h"

#include <algorithm>

#include "Renderer/Core/Renderer.h"

void ApplyEditableLightTransform(
    EditableLight& light,
    Renderer& renderer)
{
    light.proxy.position = light.transform.position;
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
