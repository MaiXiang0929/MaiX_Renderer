// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include "Renderer/Resources/RenderResourceHandle.h"
#include "Editor/EditableModel.h"

enum class MaterialEditAction { None, CreateInstance, UseParent };
struct MaterialEditRequest
{
    MaterialEditAction action = MaterialEditAction::None;
    ModelId model;
    MaterialHandle material;
};

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
              MaterialEditRequest& request,
              void* nativeWindowHandle = nullptr);

private:
    bool m_EditParent = false;
    MaterialHandle m_LastMaterial;
    std::string m_TextureMessage;
    bool m_TextureMessageIsError = false;
};
