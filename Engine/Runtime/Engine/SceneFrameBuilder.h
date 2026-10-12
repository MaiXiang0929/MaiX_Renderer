// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Runtime/Component/Camera.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"

struct SceneFrameSettings
{
    bool tessellationEnabled = false;
    float displacementScale = 0.0f;
    bool editorPrimitivesEnabled = true;
};

RenderFrameData BuildSceneFrame(SceneState& scene, Camera& camera,
    unsigned int width, unsigned int height, bool sceneCameraView,
    const SceneFrameSettings& settings);
