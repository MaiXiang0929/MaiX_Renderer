// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/Renderer/View/ShadowFrame.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Core/Math/Bounds.h"

// Inputs and output are CPU values; ShadowPass owns the depth textures.
ShadowFrame BuildShadowFrame(const std::vector<LightSceneProxy>& lights, LightId mainLight,
    const ShadowSettings& settings, const PrimitiveBounds& casterBounds,
    const cy::Matrix4f& projection, const cy::Matrix4f& view,
    const cy::Matrix4f& reflectionView, bool reflectionEnabled, float displacementPadding = 0);
