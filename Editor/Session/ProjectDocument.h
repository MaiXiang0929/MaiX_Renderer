// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/SceneDocument.h"
#include <vector>
struct ProjectDocument
{
    std::string name;
    std::filesystem::path file, root, assets, sceneFile, settingsRoot;
};
bool ReadProjectDocument(const std::filesystem::path& file,
    const std::vector<std::filesystem::path>& protectedDirectories, ProjectDocument&, std::string& error);
bool CreateProjectDocument(const std::filesystem::path& parent, const std::string& name,
    const SceneDocument& initialScene, const std::vector<std::filesystem::path>& protectedDirectories,
    ProjectDocument&, std::string& error);
// Empty existingScene creates Main only when it does not already exist.
bool InitializeLegacyProject(const std::filesystem::path& root, const std::filesystem::path& existingScene,
    const SceneDocument& initialScene, const std::vector<std::filesystem::path>& protectedDirectories,
    ProjectDocument&, std::string& error);
std::filesystem::path FindProjectDocument(const std::filesystem::path& root);
