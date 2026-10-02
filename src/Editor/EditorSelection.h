// SPDX-License-Identifier: MIT
#pragma once

#include "Editor/EditableModel.h"
#include "Editor/EditableCamera.h"
#include "Renderer/Scene/LightSceneProxy.h"

enum class EditorSelectionType
{
    None,
    Model,
    Light,
    Camera
};

struct EditorSelection
{
    EditorSelectionType type = EditorSelectionType::None;
    ModelId modelId;
    CameraId cameraId;
    LightId lightId = InvalidLightId;

    void Clear()
    {
        type = EditorSelectionType::None;
        modelId = {};
        cameraId = {};
        lightId = InvalidLightId;
    }

    void SelectModel(ModelId id)
    {
        type = id.IsValid() ? EditorSelectionType::Model : EditorSelectionType::None;
        modelId = id;
        cameraId = {};
        lightId = InvalidLightId;
    }

    void SelectLight(LightId id)
    {
        type = id == InvalidLightId
            ? EditorSelectionType::None
            : EditorSelectionType::Light;
        modelId = {};
        cameraId = {};
        lightId = id;
    }

    void SelectCamera(CameraId id)
    {
        Clear();
        if (id.IsValid()) { type = EditorSelectionType::Camera; cameraId = id; }
    }
    bool IsCameraSelected(CameraId id) const
    { return type == EditorSelectionType::Camera && cameraId == id; }

    bool IsModelSelected(ModelId id) const
    {
        return type == EditorSelectionType::Model && modelId == id;
    }

    bool IsLightSelected(LightId id) const
    {
        return type == EditorSelectionType::Light && lightId == id;
    }
};
