// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Editor/Session/ProjectDocument.h"

// Editor-owned project identity/CPU paths. Renderer ownership stays in EngineSession.
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
    // Folder requests share startup resolution. Described projects are staged in
    // StartupProjectFile for OpenProjectSession; they do not activate identity here.
    bool Open(const std::filesystem::path& existingProjectRoot);
    bool IsReady() const;
    bool ContainsAssetPath(const std::filesystem::path& path) const;
    bool PrepareOpen(const std::filesystem::path& file,ProjectDocument&,std::string& error) const;
    void Activate(ProjectDocument&&) noexcept;
    const std::vector<std::filesystem::path>& ProtectedDirectories() const { return m_ProtectedDirectories; }
    const std::filesystem::path& ProjectFile() const { return m_ProjectFile; }
    // Validated startup request; identity is committed only by OpenProjectSession.
    const std::filesystem::path& StartupProjectFile() const { return m_StartupProjectFile; }
    const std::filesystem::path& DefaultScene() const { return m_DefaultScene; }
    const std::string& Name() const { return m_Name; }
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
    std::filesystem::path m_ProjectFile,m_DefaultScene,m_RequestedFile,m_StartupProjectFile;
    std::string m_Name;
};
