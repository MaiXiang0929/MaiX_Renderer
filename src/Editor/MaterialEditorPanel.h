// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include "Renderer/Resources/RenderResourceHandle.h"

class Renderer;
class EditorMaterialSelection;
struct EditorSelection;
struct EditableModel;

class MaterialEditorPanel
{
public:
    void Draw(Renderer& renderer, const EditorSelection& selection,
              const std::vector<EditableModel>& models,
              EditorMaterialSelection& materialSelection,
              void* nativeWindowHandle = nullptr);

private:
    MaterialHandle m_LastMaterial;
    std::string m_TextureMessage;
    bool m_TextureMessageIsError = false;
};
