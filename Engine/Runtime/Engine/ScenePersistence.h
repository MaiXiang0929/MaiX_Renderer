// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneDocument.h"
class Renderer;
struct SceneState;
bool CaptureSceneDocument(SceneState&, const Renderer&, const SceneAssetRoots&, SceneDocument&, std::string& error);
// Candidate resources use the same Renderer and are rolled back before returning on failure.
bool ReplaceSceneFromDocument(SceneState&, Renderer&, const SceneAssetRoots&, const SceneDocument&, std::string& error);
void DestroySceneResources(SceneState&, Renderer&);
