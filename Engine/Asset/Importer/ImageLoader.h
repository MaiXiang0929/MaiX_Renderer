// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "Engine/Asset/Importer/ImageData.h"

namespace AssetImport
{
class ImageLoader
{
public:
    // Radiance RGBE only, 2:1, top-first linear RGBA; rejects values outside RGBA16F range.
    static bool LoadHdrFromFile(const std::filesystem::path& path, HdrImageData& image, std::string& error);
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
