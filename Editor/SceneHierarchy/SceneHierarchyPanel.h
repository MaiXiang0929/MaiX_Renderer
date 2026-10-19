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
    ActorId Draw(EditorSelection& selection, SceneState& scene);
private:
    std::string m_Error;
};
