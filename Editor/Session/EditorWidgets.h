// SPDX-License-Identifier: MIT
#pragma once
#include <imgui.h>

namespace EditorWidgets
{
enum class Icon { Folder, Mesh, Light, Camera, Move, Rotate, Scale, World, Local, File };
void DrawIcon(Icon icon, ImVec2 position, float size);
bool IconButton(const char* id, Icon icon, const char* tooltip, bool selected = false,
    const char* label = nullptr);
bool IconSelectable(const char* id, Icon icon, const char* label, bool selected);
bool ObjectHeader(Icon icon, const char* name, const char* type, bool* visible = nullptr);
bool BeginProperties(const char* id);
void PropertyLabel(const char* label);
bool Section(const char* label, bool defaultOpen = true);
struct EditLifecycle { bool activated = false, active = false, deactivated = false; };
bool DragVector3(const char* label, float* values, float speed,
    float minimum = 0.0f, float maximum = 0.0f, const char* format = "%.3f",
    EditLifecycle* lifecycle = nullptr);
// Centralized compatibility code for the vendored ImGui docking tab bars.
void DrawDockTabAccents();
}
