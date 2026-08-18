// SPDX-License-Identifier: MIT
#pragma once

#include <string>

class Renderer;

class MaterialEditorPanel
{
public:
    void Draw(Renderer& renderer, void* nativeWindowHandle = nullptr);

private:
    unsigned int m_SelectedMaterial = 0;
    std::string m_TextureMessage;
    bool m_TextureMessageIsError = false;
};
