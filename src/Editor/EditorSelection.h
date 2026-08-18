// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Scene/LightSceneProxy.h"

enum class EditorSelectionType
{
    None,
    Model,
    Light
};

struct EditorSelection
{
    EditorSelectionType type = EditorSelectionType::None;
    LightId lightId = InvalidLightId;

    void Clear()
    {
        type = EditorSelectionType::None;
        lightId = InvalidLightId;
    }

    void SelectModel()
    {
        type = EditorSelectionType::Model;
        lightId = InvalidLightId;
    }

    void SelectLight(LightId id)
    {
        type = id == InvalidLightId
            ? EditorSelectionType::None
            : EditorSelectionType::Light;
        lightId = id;
    }

    bool IsModelSelected() const
    {
        return type == EditorSelectionType::Model;
    }

    bool IsLightSelected(LightId id) const
    {
        return type == EditorSelectionType::Light && lightId == id;
    }
};
