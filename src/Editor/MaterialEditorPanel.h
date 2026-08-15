// SPDX-License-Identifier: MIT
#pragma once

class Renderer;

class MaterialEditorPanel
{
public:
    void Draw(Renderer& renderer);

private:
    unsigned int m_SelectedMaterial = 0;
};
