// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <imgui.h>

#include "Editor/Session/EditorFonts.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[EditorFontsTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}
}

void TestChineseGlyphs(const std::filesystem::path& path)
{
    ImFontAtlas atlas;
    std::string diagnostic;
    Require(EditorFonts::Initialize(atlas, path, diagnostic),
        "Bundled Chinese font should load and build.");
    Require(diagnostic.empty() && atlas.IsBuilt(),
        "Successful initialization should produce a ready atlas without warnings.");
    // FindGlyph() could return '?': require a real glyph for Chinese asset names.
    for (ImWchar character : { 0x6750, 0x8D28, 0x5934, 0x53D1, 0x8138,
            0x8EAB, 0x4F53, 0x773C, 0x775B, 0x6C38, 0x591C, 0x9AD4, 0xFF01 })
        Require(atlas.Fonts.front()->FindGlyphNoFallback(character) != nullptr,
            "Chinese material names must have real glyphs, not question-mark fallback.");

    ImFontAtlas original;
    original.AddFontDefault();
    Require(original.Build(), "Original English font should build.");
    for (ImWchar character : { 'S', 'l', 'o', 't', '1', ':', '_', '?', '+' })
    {
        const ImFontGlyph* actual = atlas.Fonts.front()->FindGlyphNoFallback(character);
        const ImFontGlyph* expected = original.Fonts.front()->FindGlyphNoFallback(character);
        Require(actual && expected && actual->AdvanceX == expected->AdvanceX,
            "Merging Chinese should preserve existing English and numeric text widths.");
    }
    std::cout << "Chinese font atlas: " << atlas.TexWidth << "x" << atlas.TexHeight
        << ", glyphs=" << atlas.Fonts.front()->Glyphs.Size << std::endl;
}

void TestMissingFontFallback(const std::filesystem::path& directory)
{
    ImFontAtlas atlas;
    std::string diagnostic;
    Require(!EditorFonts::Initialize(atlas, directory / "not-present.ttf", diagnostic),
        "A missing font should report Chinese loading failure.");
    Require(!diagnostic.empty() && atlas.IsBuilt() &&
            atlas.Fonts.front()->FindGlyphNoFallback('S') != nullptr,
        "Missing font should leave a usable English atlas and a diagnostic.");
}

void TestUnicodeFontPathAndTruncatedFile(const std::filesystem::path& source)
{
    const auto suffix = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        (std::wstring(L"MaiX_Font_中文_") + std::to_wstring(suffix) + L".ttf");
    std::filesystem::copy_file(source, path);
    ImFontAtlas atlas;
    std::string diagnostic;
    const bool loaded = EditorFonts::Initialize(atlas, path, diagnostic);
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        // Retain enough real TTF header bytes to enter ImGui/STB parsing if
        // the loader only checks the minimum (>100-byte) file length.
        std::ifstream original(source, std::ios::binary);
        char prefix[4096];
        original.read(prefix, sizeof(prefix));
        stream.write(prefix, sizeof(prefix));
        Require(static_cast<bool>(stream), "Truncated font fixture should be written.");
    }
    const bool truncatedLoaded = EditorFonts::Initialize(atlas, path, diagnostic);
    Require(!truncatedLoaded && !diagnostic.empty() && atlas.IsBuilt() &&
            atlas.Fonts.front()->FindGlyphNoFallback('S') != nullptr,
        "Truncated font should fall back without leaving a partial atlas.");
    std::filesystem::copy_file(source, path, std::filesystem::copy_options::overwrite_existing);
    {
        // Corruption with unchanged file size must also be rejected before parsing.
        std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
        stream.put('x');
        Require(static_cast<bool>(stream), "Corrupt font fixture should be written.");
    }
    const bool corruptedLoaded = EditorFonts::Initialize(atlas, path, diagnostic);
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    Require(loaded, "Font loading should support Unicode filesystem paths.");
    Require(!corruptedLoaded && !diagnostic.empty() && atlas.IsBuilt() &&
            atlas.Fonts.front()->FindGlyphNoFallback('S') != nullptr,
        "Same-size corrupt font should fall back without entering unsafe parsing.");
}

int main(int argc, char** argv)
{
    Require(argc == 2, "Pass the bundled font file path.");
    const auto path = std::filesystem::u8path(argv[1]);
    TestChineseGlyphs(path);
    TestMissingFontFallback(path.parent_path());
    TestUnicodeFontPathAndTruncatedFile(path);
    std::cout << "Editor font tests passed." << std::endl;
    return EXIT_SUCCESS;
}
