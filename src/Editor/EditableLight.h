// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "Core/Transform.h"
#include "Renderer/Scene/LightSceneProxy.h"

struct EditableLight
{
    std::string name;
    Transform transform;
    LightSceneProxy proxy;

    bool IsValid() const { return proxy.id != InvalidLightId; }
};

void ApplyEditableLightTransform(
    EditableLight& light,
    class Renderer& renderer);

EditableLight* FindEditableLight(
    std::vector<EditableLight>& lights,
    LightId id);
const EditableLight* FindEditableLight(
    const std::vector<EditableLight>& lights,
    LightId id);
