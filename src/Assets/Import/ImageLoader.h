// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "Assets/Import/ImageData.h"

namespace AssetImport
{
class ImageLoader
{
public:
    static bool LoadFromFile(
        const std::filesystem::path& path,
        ImageData& outImage,
        std::string& outError);

    static bool LoadFromMemory(
        const unsigned char* encodedData,
        std::size_t encodedSize,
        ImageData& outImage,
        std::string& outError);

    static bool LoadFromMemory(
        const std::vector<std::uint8_t>& encodedData,
        ImageData& outImage,
        std::string& outError);
};
}
