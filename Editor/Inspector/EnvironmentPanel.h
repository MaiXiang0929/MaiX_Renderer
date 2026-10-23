// SPDX-License-Identifier: MIT
#pragma once
struct SceneState;
class Renderer;
// Drawn inside Render Settings; only the renderer owns environment GPU resources.
void DrawEnvironmentControls(SceneState& scene, Renderer& renderer, void* nativeWindow);
