// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneModel.h"
#include "Engine/Runtime/Scene/SceneLight.h"
class Renderer;

void ApplySceneModelTransform(SceneModel& model, Renderer& renderer);
void ApplySceneLightTransform(SceneLight& light, Renderer& renderer);
// Unregister primitives first; release meshes and child materials before parents.
void DestroySceneModelResources(SceneModel& model, Renderer& renderer);
