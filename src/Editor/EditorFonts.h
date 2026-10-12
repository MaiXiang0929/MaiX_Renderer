// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>

struct ImFontAtlas;

namespace EditorFonts
{
// Loads the pinned, bundled Noto Sans SC asset only (not an arbitrary font loader).
// Initialization only: replaces the atlas and builds CPU pixels before the first frame.
// ImGui owns font data; its OpenGL backend owns the uploaded texture.
// False means Chinese loading failed; a built default atlas remains available.
bool Initialize(ImFontAtlas& atlas, const std::filesystem::path& fontPath,
    std::string& diagnostic);
}
