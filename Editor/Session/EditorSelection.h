// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/Runtime/Scene/SceneModel.h"
#include "Engine/Runtime/Scene/SceneCamera.h"
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"
#include "Engine/Runtime/Engine/SceneState.h"

enum class EditorSelectionType
{
    None,
    Model,
    Light,
    Camera,
    Empty
};

struct EditorSelection
{
    EditorSelectionType type = EditorSelectionType::None;
    ModelId modelId;
    CameraId cameraId;
    LightId lightId = InvalidLightId;
    ActorId emptyActor;

    void Clear()
    {
        type = EditorSelectionType::None;
        modelId = {};
        cameraId = {};
        lightId = InvalidLightId;
        emptyActor = {};
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
        type = id == InvalidLightId ? EditorSelectionType::None : EditorSelectionType::Light;
        modelId = {};
        cameraId = {};
        lightId = id;
    }

    void SelectCamera(CameraId id)
    {
        Clear();
        if (id.IsValid())
        {
            type = EditorSelectionType::Camera;
            cameraId = id;
        }
    }
    bool IsCameraSelected(CameraId id) const
    {
        return type == EditorSelectionType::Camera && id.IsValid() && cameraId == id;
    }

    bool IsModelSelected(ModelId id) const
    {
        return type == EditorSelectionType::Model && modelId == id;
    }

    bool IsLightSelected(LightId id) const
    {
        return type == EditorSelectionType::Light && lightId == id;
    }

    ActorId ResolveActor(const SceneState &scene) const
    {
        if (type == EditorSelectionType::Empty)
            return scene.world.Find(emptyActor) ? emptyActor : ActorId{};
        if (type == EditorSelectionType::Model)
        {
            const auto *m = FindSceneModel(scene.models, modelId);
            return m ? m->actor : ActorId{};
        }
        if (type == EditorSelectionType::Light)
        {
            const auto *l = FindSceneLight(scene.lights, lightId);
            return l ? l->actor : ActorId{};
        }
        return type == EditorSelectionType::Camera && cameraId == scene.camera.id ? scene.camera.actor
                                                                                  : ActorId{};
    }
    void SelectActor(ActorId id, const SceneState &scene)
    {
        const auto *a = scene.world.Find(id);
        Clear();
        if (!a)
            return;
        switch (a->kind)
        {
        case ActorKind::Model:
            SelectModel(ModelId{a->componentId});
            break;
        case ActorKind::Light:
            SelectLight(static_cast<LightId>(a->componentId));
            break;
        case ActorKind::Camera:
            SelectCamera(CameraId{a->componentId});
            break;
        case ActorKind::Empty:
            type = EditorSelectionType::Empty;
            emptyActor = id;
            break;
        }
    }
};
