// SPDX-License-Identifier: MIT
#include "EditorFonts.h"

#include <fstream>
#include <cstdint>
#include <vector>

#include <imgui.h>

namespace EditorFonts
{
namespace
{
// Pin the shipped asset before handing it to STB, which does not bounds-check
// malformed font tables. Update these together when deliberately upgrading it.
constexpr std::streamoff BundledFontSize = 17772300;
constexpr std::uint64_t BundledFontHash = UINT64_C(0x2adecf5b9049c2ad);

bool HasBundledFontIntegrity(const std::vector<unsigned char>& bytes)
{
    // FNV-1a is an accidental-corruption check, not a security/authenticity check.
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : bytes)
        hash = (hash ^ byte) * UINT64_C(1099511628211);
    return hash == BundledFontHash;
}

// Include basic/extension-A ideographs and punctuation, rather than only the
// 2,500 common characters: imported names can contain traditional Chinese.
// Static storage is required because ImGui retains the range pointer.
constexpr ImWchar ChineseRanges[] = {
    0x2000, 0x206F,
    0x3000, 0x30FF,
    0x31F0, 0x31FF,
    0x3400, 0x4DBF,
    0x4E00, 0x9FFF,
    0xFF00, 0xFFEF,
    0xFFFD, 0xFFFD,
    0
};
}

bool Initialize(ImFontAtlas& atlas, const std::filesystem::path& fontPath,
    std::string& diagnostic)
{
    diagnostic.clear();
    atlas.Clear();
    atlas.AddFontDefault();

    const auto fallback = [&](const char* reason)
    {
        atlas.Clear();
        atlas.AddFontDefault();
        const bool built = atlas.Build();
        diagnostic = std::string(reason) + ": " + fontPath.u8string() +
            ". Using the default font; Chinese names may display as '?'.";
        if (!built)
            diagnostic += " Default font atlas build also failed.";
        return false;
    };

    // Read through filesystem::path so Unicode paths work on Windows. Checking
    // I/O here also avoids AddFontFromFileTTF's debug assertion for missing files.
    std::ifstream stream(fontPath, std::ios::binary | std::ios::ate);
    if (!stream)
        return fallback("Unable to open editor Chinese font");
    const std::streamoff size = stream.tellg();
    if (size != BundledFontSize)
        return fallback("Editor Chinese font is incomplete or differs from the bundled asset");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size))
        return fallback("Unable to read editor Chinese font");
    if (!HasBundledFontIntegrity(bytes))
        return fallback("Editor Chinese font integrity check failed");

    ImFontConfig config;
    config.MergeMode = true;
    // ImGui copies these bytes into atlas-owned storage before bytes goes away.
    config.FontDataOwnedByAtlas = false;
    config.OversampleH = config.OversampleV = 1;
    if (atlas.AddFontFromMemoryTTF(bytes.data(), static_cast<int>(size),
            13.0f, &config, ChineseRanges) == nullptr || !atlas.Build())
        return fallback("Unable to build editor Chinese font atlas");
    return true;
}
}
