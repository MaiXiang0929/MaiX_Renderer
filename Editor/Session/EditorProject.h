// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Editor-owned CPU paths only. This is a content workspace, not a scene or asset database.
class EditorProject
{
public:
    EditorProject(std::filesystem::path defaultRoot,
        std::vector<std::filesystem::path> protectedDirectories);
    EditorProject(const EditorProject&) = delete;
    EditorProject& operator=(const EditorProject&) = delete;
    // Register before loading ImGui settings; the object outlives the context.
    void RegisterSettings();
    bool Initialize();
    bool Open(const std::filesystem::path& existingProjectRoot);
    bool IsReady() const;
    bool ContainsAssetPath(const std::filesystem::path& path) const;
    const std::filesystem::path& Root() const { return m_Root; }
    const std::filesystem::path& AssetsRoot() const { return m_AssetsRoot; }
    const std::string& Diagnostic() const { return m_Diagnostic; }

private:
    bool TryOpen(const std::filesystem::path& root, bool createProjectRoot);
    bool IsProtected(const std::filesystem::path& canonicalPath) const;
    std::filesystem::path m_DefaultRoot;
    std::vector<std::filesystem::path> m_ProtectedDirectories;
    std::filesystem::path m_RequestedRoot;
    std::filesystem::path m_Root;
    std::filesystem::path m_AssetsRoot;
    bool m_HasSavedRoot = false;
    std::string m_Diagnostic;
};
