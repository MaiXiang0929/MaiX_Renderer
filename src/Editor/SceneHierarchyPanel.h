// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Editor/EditorSelection.h"

struct EditableLight;
struct EditableModel;

class SceneHierarchyPanel
{
public:
    void Draw(
        EditorSelection& selection,
        const EditableModel& model,
        const std::vector<EditableLight>& lights);
};
