// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Editor/Session/EditorSelection.h"

struct SceneLight;
struct SceneModel;
class Renderer;
class EditorMaterialSelection;

class InspectorPanel
{
public:
    bool Draw(
        EditorSelection& selection,
        std::vector<SceneModel>& models,
        std::vector<SceneLight>& lights,
        SceneCamera& camera, bool& cameraView,
        Renderer& renderer,
        EditorMaterialSelection& materialSelection);
};
