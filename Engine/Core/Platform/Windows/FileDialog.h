// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <optional>

namespace FileDialog
{
using NativeWindowHandle = void*;

// Opens a native Windows picker without copying or importing the selected file.
std::optional<std::filesystem::path> OpenModelFile(
    NativeWindowHandle ownerWindow = nullptr);
std::optional<std::filesystem::path> OpenHdrEnvironmentFile(NativeWindowHandle ownerWindow = nullptr);
std::optional<std::filesystem::path> OpenTextureFile(
    NativeWindowHandle ownerWindow = nullptr);
std::optional<std::filesystem::path> OpenSceneFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory);
std::optional<std::filesystem::path> SaveSceneFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory);
std::optional<std::filesystem::path> OpenWorldFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory);
std::optional<std::filesystem::path> SaveWorldFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory);
std::optional<std::filesystem::path> OpenProjectFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory);
std::optional<std::filesystem::path> OpenDirectory(
    NativeWindowHandle ownerWindow = nullptr,
    const std::filesystem::path& initialDirectory = {});
}
