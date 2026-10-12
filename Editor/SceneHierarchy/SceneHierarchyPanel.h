// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Editor/Session/EditorSelection.h"

struct SceneLight;
struct SceneModel;
struct ModelId;

class SceneHierarchyPanel
{
public:
    ModelId Draw(
        EditorSelection& selection,
        const std::vector<SceneModel>& models,
        const std::vector<SceneLight>& lights, const SceneCamera* camera);
};
