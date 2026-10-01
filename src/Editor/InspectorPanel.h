// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Editor/EditorSelection.h"

struct EditableLight;
struct EditableModel;
class Renderer;
class EditorMaterialSelection;

class InspectorPanel
{
public:
    bool Draw(
        EditorSelection& selection,
        std::vector<EditableModel>& models,
        std::vector<EditableLight>& lights,
        Renderer& renderer,
        EditorMaterialSelection& materialSelection);
};
