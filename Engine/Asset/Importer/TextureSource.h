// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include "Engine/Asset/Importer/ImageData.h"

namespace AssetImport
{
// CPU reconstruction data only. No GPU handles or project-root policy.
enum class TextureSourceKind { Image, Normal, Orm };
struct TextureSourceImage
{
    std::filesystem::path path;
    std::uint64_t embeddedId = 0;
};
struct TextureChannelInput
{
    TextureSourceImage image;
    unsigned channel = 0; // RGBA = 0,1,2,3.
};
struct TextureSource
{
    TextureSourceKind kind = TextureSourceKind::Image;
    TextureSourceImage image; // Image/Normal only.
    bool flipGreen = false; // Normal only; authored RGB is otherwise unchanged.
    std::array<std::optional<TextureChannelInput>,3> ormInputs; // AO, roughness, metallic.
    unsigned width = 0, height = 0; // Orm output dimensions only.
};
using TextureImageLoader = std::function<bool(const TextureSourceImage&, ImageData&, std::string&)>;
bool ValidateTextureSource(const TextureSource& source, std::string& error);
bool BuildTextureSourceImage(const TextureSource& source, const TextureImageLoader& loader,
                             ImageData& output, std::string& error);
}
