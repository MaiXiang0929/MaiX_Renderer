// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneModel.h"
#include "Engine/Runtime/Scene/SceneLight.h"
class Renderer;
struct SceneState;

bool RegisterSceneActors(SceneState& scene);
void SynchronizeSceneActors(SceneState& scene, Renderer& renderer);
bool DestroySceneActor(SceneState& scene, Renderer& renderer, ActorId actor);
// Creates a root Actor and its light proxy together; failure leaves the scene unchanged.
LightId CreateSceneLight(SceneState& scene, Renderer& renderer, LightType type,
                        const Transform& local, std::string& error);

void ApplySceneModelTransform(SceneModel& model, Renderer& renderer);
void ApplySceneLightTransform(SceneLight& light, Renderer& renderer);
// Unregister primitives first; release meshes and child materials before parents.
void DestroySceneModelResources(SceneModel& model, Renderer& renderer);
