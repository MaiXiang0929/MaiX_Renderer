// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/World.h"
#include "Engine/Rendering/Renderer/Resources/RenderResourceHandle.h"
#include <memory>
struct SceneState;
class Renderer;
struct SceneSubtreeArchive;
struct ModelId;
// CPU archive + opaque renderer retention identity. Explicitly release before the renderer/context.
bool DetachSceneSubtree(SceneState& scene, Renderer& renderer, ActorId root,
    std::shared_ptr<SceneSubtreeArchive>& archive, std::string& error);
bool RestoreSceneSubtree(SceneState& scene, Renderer& renderer, SceneSubtreeArchive& archive, std::string& error);
void ReleaseSceneSubtree(SceneState& scene, Renderer& renderer, std::shared_ptr<SceneSubtreeArchive>& archive) noexcept;
std::vector<MaterialHandle> GetSceneMaterialHandles(const SceneState& scene, const Renderer& renderer);
void ReleaseUnusedModelMaterial(SceneState& scene, Renderer& renderer, ModelId owner, MaterialHandle material);
