// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneCamera.h"
#include "Engine/Runtime/Scene/SceneLight.h"
#include "Engine/Runtime/Scene/SceneModel.h"
#include <limits>
#include <vector>

// Transitional scene values and render bindings; no UI state or GPU ownership.
// World / Actor / Component replace this organization in Phase 2.
struct SceneState
{
    std::vector<SceneModel> models;
    std::vector<SceneLight> lights;
    SceneCamera camera;
    std::uint64_t nextModelId = 1;
    ModelId defaultModelId;
    LightId mainLightId = InvalidLightId;
    bool reflectionGroundEnabled = false;
    float sceneRadius = 1.0f;
    cy::Vec3f objCenter{0.0f};
    cy::Vec3f defaultBoundsMin{0.0f};
    cy::Vec3f defaultBoundsMax{0.0f};
    float groundY = 0.0f;
    float modelDiameter = 0.0f;
};
