// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Editor/EditorSelection.h"

struct EditableLight;
struct EditableModel;
class Renderer;

class InspectorPanel
{
public:
    void Draw(
        EditorSelection& selection,
        EditableModel& model,
        std::vector<EditableLight>& lights,
        Renderer& renderer);
};
