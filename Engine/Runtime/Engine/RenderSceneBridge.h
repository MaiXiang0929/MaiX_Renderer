// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneModel.h"
#include "Engine/Runtime/Scene/SceneLight.h"
class Renderer;
struct SceneState;

bool RegisterSceneActors(SceneState& scene);
void SynchronizeSceneActors(SceneState& scene, Renderer& renderer);
bool DestroySceneActor(SceneState& scene, Renderer& renderer, ActorId actor);

void ApplySceneModelTransform(SceneModel& model, Renderer& renderer);
void ApplySceneLightTransform(SceneLight& light, Renderer& renderer);
// Unregister primitives first; release meshes and child materials before parents.
void DestroySceneModelResources(SceneModel& model, Renderer& renderer);
