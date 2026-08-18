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
std::optional<std::filesystem::path> OpenTextureFile(
    NativeWindowHandle ownerWindow = nullptr);
}
