// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneCamera.h"
#include "Engine/Runtime/Scene/SceneLight.h"
#include "Engine/Runtime/Scene/SceneModel.h"
#include <limits>
#include <vector>

// World owns identity/local transforms; typed component records below expose
// derived world transforms to the existing render bridge. No UI/GPU ownership.
struct SceneState
{
    World world;
    std::vector<SceneModel> models;
    std::vector<SceneLight> lights;
    // Scene-file material table includes unbound bases and parents of instances.
    std::vector<MaterialHandle> sceneMaterials;
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
