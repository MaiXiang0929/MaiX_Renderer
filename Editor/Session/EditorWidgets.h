// SPDX-License-Identifier: MIT
#pragma once
#include <imgui.h>

namespace EditorWidgets
{
enum class Icon { Folder, Mesh, Light, Camera, Move, Rotate, Scale, World, Local, File };
void DrawIcon(Icon icon, ImVec2 position, float size);
bool IconButton(const char* id, Icon icon, const char* tooltip, bool selected = false);
bool IconSelectable(const char* id, Icon icon, const char* label, bool selected);
void ObjectHeader(Icon icon, const char* name, const char* type);
bool Section(const char* label, bool defaultOpen = true);
bool DragVector3(const char* label, float* values, float speed,
    float minimum = 0.0f, float maximum = 0.0f, const char* format = "%.3f");
// Centralized compatibility code for the vendored ImGui docking tab bars.
void DrawDockTabAccents();
}
