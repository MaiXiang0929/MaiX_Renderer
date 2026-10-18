// SPDX-License-Identifier: MIT
#pragma once
#include <imgui.h>

enum class EditorThemeMode { Light, Dark };

struct EditorAppearanceSettings
{
    EditorThemeMode theme = EditorThemeMode::Dark;
    float uiScale = 1.0f;
};

namespace EditorTheme
{
struct Palette
{
    ImVec4 background, panel, header, field, text, muted, border;
    ImVec4 blue, selected, pink, folder, icon, error, warning;
};

// Settings storage is owned by EditorApp and must outlive the ImGui context.
void RegisterSettings(EditorAppearanceSettings& settings);
void Apply(EditorThemeMode mode, float scale);
const Palette& GetPalette();
float GetScale();
}
