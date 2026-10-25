// SPDX-License-Identifier: MIT
#pragma once

#include <vector>
#include <functional>

#include "Editor/Session/EditorSelection.h"

struct SceneLight;
struct SceneModel;
struct ModelId;

class SceneHierarchyPanel
{
public:
    ActorId Draw(EditorSelection& selection, SceneState& scene, class Renderer& renderer,
        class EditorHistory* history = nullptr, const std::function<void(const char*)>& invalidate = {});
private:
    std::string m_Error;
};
