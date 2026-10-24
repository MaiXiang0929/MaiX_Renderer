// SPDX-License-Identifier: MIT
#include "Editor/Session/EditorTheme.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace EditorTheme
{
namespace
{
Palette palette;
float appliedScale = 1.0f;
ImVec4 Hex(unsigned int color)
{
    return ImVec4(((color >> 16) & 255) / 255.0f,
        ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f, 1.0f);
}
ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t, 1.0f);
}
}

void RegisterSettings(EditorAppearanceSettings& settings)
{
    ImGuiSettingsHandler handler;
    handler.TypeName = "MaiXAppearance";
    handler.TypeHash = ImHashStr(handler.TypeName);
    handler.UserData = &settings;
    handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* h) {
        *static_cast<EditorAppearanceSettings*>(h->UserData) = {};
    };
    handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* h, const char* name) -> void* {
        return std::strcmp(name, "Editor") == 0 ? h->UserData : nullptr;
    };
    handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
        auto& value = *static_cast<EditorAppearanceSettings*>(entry);
        if (std::strncmp(line, "Theme=", 6) == 0)
            value.theme = std::strcmp(line + 6, "Light") == 0 ? EditorThemeMode::Light : EditorThemeMode::Dark;
        if (std::strncmp(line, "Scale=", 6) == 0)
        {
            char* end = nullptr;
            const float scale = std::strtof(line + 6, &end);
            value.uiScale = end != line + 6 && *end == '\0' && std::isfinite(scale) &&
                scale >= 0.75f && scale <= 2.0f ? scale : 1.0f;
        }
    };
    handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buffer) {
        const auto& value = *static_cast<EditorAppearanceSettings*>(h->UserData);
        buffer->appendf("[MaiXAppearance][Editor]\nTheme=%s\nScale=%.3f\n\n",
            value.theme == EditorThemeMode::Light ? "Light" : "Dark", value.uiScale);
    };
    ImGui::AddSettingsHandler(&handler);
}

void Apply(EditorThemeMode mode, float scale)
{
    const bool light = mode == EditorThemeMode::Light;
    palette = light ? Palette{
        Hex(0xEAF1F7), Hex(0xF8FAFC), Hex(0xDFEAF4), Hex(0xFCFDFE),
        Hex(0x3E5064), Hex(0x6B7C90), Hex(0xCEDBE7), Hex(0x719CC6),
        Hex(0xD4E5F5), Hex(0xE5B4C5), Hex(0x8FB6DA), Hex(0x617D9A),
        Hex(0xB34552), Hex(0x956923)
    } : Palette{
        Hex(0x242B36), Hex(0x2D3643), Hex(0x374352), Hex(0x252E3A),
        Hex(0xE3E9F0), Hex(0xABB9CA), Hex(0x455263), Hex(0x9CBFE3),
        Hex(0x435D79), Hex(0xE4B2C6), Hex(0x9CBFE3), Hex(0xADBED2),
        Hex(0xF298A0), Hex(0xE4C18B)
    };
    // Always start from unscaled metrics; switching themes must be idempotent.
    ImGuiStyle style;
    auto* c = style.Colors;
    const auto& p = palette;
    c[ImGuiCol_Text] = p.text;
    c[ImGuiCol_TextDisabled] = p.muted;
    c[ImGuiCol_WindowBg] = c[ImGuiCol_ChildBg] = c[ImGuiCol_PopupBg] = p.panel;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_BorderShadow] = ImVec4(0,0,0,0);
    c[ImGuiCol_FrameBg] = p.field;
    c[ImGuiCol_FrameBgHovered] = Mix(p.field, p.selected, 0.55f);
    c[ImGuiCol_FrameBgActive] = p.selected;
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = c[ImGuiCol_TitleBgCollapsed] = p.header;
    c[ImGuiCol_MenuBarBg] = p.background;
    c[ImGuiCol_ScrollbarBg] = p.panel;
    c[ImGuiCol_ScrollbarGrab] = p.border;
    c[ImGuiCol_ScrollbarGrabHovered] = p.icon;
    c[ImGuiCol_ScrollbarGrabActive] = p.blue;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_SliderGrabActive] = p.blue;
    c[ImGuiCol_Button] = p.header;
    c[ImGuiCol_ButtonHovered] = Mix(p.header, p.selected, 0.65f);
    c[ImGuiCol_ButtonActive] = p.selected;
    c[ImGuiCol_Header] = p.selected;
    c[ImGuiCol_HeaderHovered] = Mix(p.header, p.selected, 0.55f);
    c[ImGuiCol_HeaderActive] = p.selected;
    c[ImGuiCol_Separator] = p.border;
    c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_SeparatorActive] = p.blue;
    c[ImGuiCol_ResizeGrip] = ImVec4(0,0,0,0);
    c[ImGuiCol_ResizeGripHovered] = c[ImGuiCol_ResizeGripActive] = p.blue;
    c[ImGuiCol_Tab] = c[ImGuiCol_TabDimmed] = p.header;
    c[ImGuiCol_TabSelected] = c[ImGuiCol_TabDimmedSelected] = p.panel;
    c[ImGuiCol_TabHovered] = p.selected;
    c[ImGuiCol_TabSelectedOverline] = c[ImGuiCol_TabDimmedSelectedOverline] = p.blue;
    c[ImGuiCol_DockingPreview] = ImVec4(p.blue.x, p.blue.y, p.blue.z, 0.4f);
    c[ImGuiCol_DockingEmptyBg] = p.background;
    c[ImGuiCol_TableHeaderBg] = p.header;
    c[ImGuiCol_TableBorderStrong] = c[ImGuiCol_TableBorderLight] = p.border;
    c[ImGuiCol_TableRowBg] = ImVec4(0,0,0,0);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(p.blue.x,p.blue.y,p.blue.z,0.04f);
    c[ImGuiCol_PlotLines] = c[ImGuiCol_PlotHistogram] = p.blue;
    c[ImGuiCol_PlotLinesHovered] = c[ImGuiCol_PlotHistogramHovered] = p.pink;
    c[ImGuiCol_TextSelectedBg] = p.selected;
    c[ImGuiCol_TextLink] = p.blue;
    c[ImGuiCol_DragDropTarget] = c[ImGuiCol_NavCursor] = p.blue;
    c[ImGuiCol_NavWindowingHighlight] = p.blue;
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.1f,0.15f,0.2f,0.25f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.1f,0.15f,0.2f,0.4f);
    style.WindowPadding = ImVec2(8,6);
    style.FramePadding = ImVec2(6,3);
    style.ItemSpacing = ImVec2(6,5);
    style.ItemInnerSpacing = ImVec2(5,3);
    style.CellPadding = ImVec2(4,3);
    style.WindowRounding = 3;
    style.ChildRounding = style.FrameRounding = style.PopupRounding = 3;
    style.GrabRounding = style.ScrollbarRounding = 3;
    style.TabRounding = 2;
    style.FrameBorderSize = 1;
    style.WindowBorderSize = style.ChildBorderSize = 1;
    style.TabBorderSize = 0;
    style.TabBarOverlineSize = 0; // EditorWidgets draws the bottom accent.
    style.DockingSeparatorSize = 4;
    style.ScrollbarSize = 12;
    appliedScale = std::isfinite(scale) ? std::clamp(scale, 0.75f, 4.0f) : 1.0f;
    style.ScaleAllSizes(appliedScale);
    ImGui::GetStyle() = style;
}

const Palette& GetPalette() { return palette; }
float GetScale() { return appliedScale; }
}
