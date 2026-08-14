// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr std::uint32_t MaximumInstanceGridSize = 32;

struct ApplicationOptions
{
    std::string normalMapPath;
    std::string displacementMapPath;
    std::uint32_t instanceGridSize = 0;
    bool materialLab = false;
    bool showHelp = false;
};

bool ParseApplicationOptions(
    const std::vector<std::string>& arguments,
    ApplicationOptions& options,
    std::string& errorMessage);

const char* GetApplicationUsage();
