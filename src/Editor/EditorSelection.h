// SPDX-License-Identifier: MIT
#pragma once

#include "Editor/EditableModel.h"
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
    ModelId modelId;
    LightId lightId = InvalidLightId;

    void Clear()
    {
        type = EditorSelectionType::None;
        modelId = {};
        lightId = InvalidLightId;
    }

    void SelectModel(ModelId id)
    {
        type = id.IsValid() ? EditorSelectionType::Model : EditorSelectionType::None;
        modelId = id;
        lightId = InvalidLightId;
    }

    void SelectLight(LightId id)
    {
        type = id == InvalidLightId
            ? EditorSelectionType::None
            : EditorSelectionType::Light;
        modelId = {};
        lightId = id;
    }

    bool IsModelSelected(ModelId id) const
    {
        return type == EditorSelectionType::Model && modelId == id;
    }

    bool IsLightSelected(LightId id) const
    {
        return type == EditorSelectionType::Light && lightId == id;
    }
};
