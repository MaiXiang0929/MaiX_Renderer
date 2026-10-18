// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>

struct ImFontAtlas;

namespace EditorFonts
{
// Loads the pinned, bundled Noto Sans SC asset only (not an arbitrary font loader).
// Replaces the atlas before NewFrame (never while drawing). Font 0 is body text,
// font 1 is the embedded monospace numeric font. Recreate backend texture on reload.
// ImGui owns font data; its OpenGL backend owns the uploaded texture.
// False means Chinese loading failed; a built default atlas remains available.
bool Initialize(ImFontAtlas& atlas, const std::filesystem::path& fontPath,
    std::string& diagnostic, float pixelSize = 15.0f);
}
