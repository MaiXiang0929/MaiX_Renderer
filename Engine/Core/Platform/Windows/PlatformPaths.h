// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>

namespace PlatformPaths
{
// Empty path means Windows could not resolve the location; callers report failure.
std::filesystem::path DocumentsDirectory();
std::filesystem::path ExecutableDirectory();
}
