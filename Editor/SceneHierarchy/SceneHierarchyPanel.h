// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Editor/Session/EditorSelection.h"

struct EditableLight;
struct EditableModel;
struct ModelId;

class SceneHierarchyPanel
{
public:
    ModelId Draw(
        EditorSelection& selection,
        const std::vector<EditableModel>& models,
        const std::vector<EditableLight>& lights, const EditableCamera* camera);
};
