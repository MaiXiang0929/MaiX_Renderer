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
        SceneState& scene, bool& cameraView,
        Renderer& renderer,
        EditorMaterialSelection& materialSelection, class EditorHistory* history = nullptr);
private:
    std::string m_Error;
};
