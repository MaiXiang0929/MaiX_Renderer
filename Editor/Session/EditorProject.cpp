// SPDX-License-Identifier: MIT
#include "Editor/Session/EditorProject.h"
#include <imgui_internal.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <system_error>
#include <utility>

namespace
{
namespace fs = std::filesystem;

bool SameComponent(const fs::path& a, const fs::path& b)
{
#if defined(_WIN32)
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}

bool IsWithin(const fs::path& root, const fs::path& path)
{
    auto candidate = path.begin();
    for (auto component = root.begin(); component != root.end(); ++component, ++candidate)
        if (candidate == path.end() || !SameComponent(*component, *candidate)) return false;
    return !root.empty();
}

bool CanWriteFiles(const fs::path& directory)
{
    // Exclusive creation avoids overwriting an existing file, even on a name collision.
    static unsigned int sequence = 0;
    const auto name = ".maix-write-probe-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
        "-" + std::to_string(++sequence) + ".tmp";
    const auto path = directory / name;
    FILE* file = nullptr;
#if defined(_WIN32)
    if (_wfopen_s(&file, path.c_str(), L"wbx") != 0 || !file) return false;
#else
    file = std::fopen(path.c_str(), "wbx");
    if (!file) return false;
#endif
    const bool closed = std::fclose(file) == 0;
    std::error_code error;
    const bool removed = fs::remove(path, error);
    return closed && removed && !error;
}
}

EditorProject::EditorProject(std::filesystem::path defaultRoot,
    std::vector<std::filesystem::path> protectedDirectories)
    : m_DefaultRoot(std::move(defaultRoot)), m_ProtectedDirectories(std::move(protectedDirectories))
{
}

void EditorProject::RegisterSettings()
{
    ImGuiSettingsHandler handler;
    handler.TypeName = "MaiXProject";
    handler.TypeHash = ImHashStr(handler.TypeName);
    handler.UserData = this;
    handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* h) {
        auto& project = *static_cast<EditorProject*>(h->UserData);
        project.m_RequestedRoot.clear();
        project.m_HasSavedRoot = false;
    };
    handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* h, const char* name) -> void* {
        return std::strcmp(name, "Editor") == 0 ? h->UserData : nullptr;
    };
    handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
        if (std::strncmp(line, "Root=", 5) != 0) return;
        auto& project = *static_cast<EditorProject*>(entry);
        project.m_HasSavedRoot = true;
        try { project.m_RequestedRoot = std::filesystem::u8path(line + 5); }
        catch (const std::filesystem::filesystem_error&) { project.m_RequestedRoot.clear(); }
    };
    handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buffer) {
        const auto& project = *static_cast<EditorProject*>(h->UserData);
        if (project.m_HasSavedRoot)
            buffer->appendf("[MaiXProject][Editor]\nRoot=%s\n\n", project.m_RequestedRoot.u8string().c_str());
    };
    ImGui::AddSettingsHandler(&handler);
}

bool EditorProject::Initialize()
{
    m_Root.clear();
    m_AssetsRoot.clear();
    const auto requested = m_HasSavedRoot ? m_RequestedRoot : m_DefaultRoot;
    return TryOpen(requested, !m_HasSavedRoot);
}

bool EditorProject::Open(const std::filesystem::path& root)
{
    return TryOpen(root, false);
}

bool EditorProject::IsProtected(const std::filesystem::path& canonicalPath) const
{
    for (const auto& directory : m_ProtectedDirectories)
    {
        if (directory.empty()) continue;
        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(directory, error);
        if (!error && IsWithin(root, canonicalPath)) return true;
    }
    return false;
}

bool EditorProject::TryOpen(const std::filesystem::path& requested, bool createProjectRoot)
{
    namespace fs = std::filesystem;
    const auto fail = [&](const std::string& reason) {
        m_Diagnostic = reason + "\n" + requested.u8string();
        return false;
    };
    if (requested.empty() || !requested.is_absolute())
        return fail("Choose an absolute project folder. The Documents location may be unavailable.");
    std::error_code error;
    const auto root = fs::weakly_canonical(requested, error);
    if (error) return fail("Cannot resolve the project folder: " + error.message());
    const auto assets = fs::weakly_canonical(root / "Assets", error);
    if (error) return fail("Cannot resolve the project Assets folder: " + error.message());
    if (IsProtected(root) || IsProtected(assets))
        return fail("Choose a project outside engine assets and build/deployment directories.");
    if (createProjectRoot) fs::create_directories(root, error);
    if (error || !fs::is_directory(root, error))
        return fail("The project folder is missing or inaccessible. Choose an existing project folder.");
    fs::create_directories(assets, error);
    if (error || !fs::is_directory(assets, error))
        return fail("Cannot create or open the project's Assets folder: " + error.message());
    if (!CanWriteFiles(assets)) return fail("The project's Assets folder is not writable.");
    m_Root = root;
    m_AssetsRoot = assets;
    m_RequestedRoot = root;
    m_HasSavedRoot = true;
    m_Diagnostic.clear();
    if (ImGui::GetCurrentContext()) ImGui::MarkIniSettingsDirty();
    return true;
}

bool EditorProject::IsReady() const
{
    std::error_code error;
    return !m_AssetsRoot.empty() && std::filesystem::is_directory(m_AssetsRoot, error) && !error;
}

bool EditorProject::ContainsAssetPath(const std::filesystem::path& path) const
{
    if (m_AssetsRoot.empty() || !path.is_absolute()) return false;
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    return !error && IsWithin(m_AssetsRoot, canonical) && !IsProtected(canonical);
}
