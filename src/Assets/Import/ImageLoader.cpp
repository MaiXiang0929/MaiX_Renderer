// SPDX-License-Identifier: MIT
#include "Assets/Import/ImageLoader.h"

#include <fstream>
#include <limits>
#include <memory>

#define STBI_FAILURE_USERMSG
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
std::string PathForError(const std::filesystem::path& path)
{
    return path.u8string();
}

void ResetResult(AssetImport::ImageData& outImage, std::string& outError)
{
    outImage = {};
    outError.clear();
}

std::string DecodeFailureMessage()
{
    const char* reason = stbi_failure_reason();
    return reason != nullptr
        ? std::string("Image decode failed: ") + reason
        : "Image decode failed: unknown image data.";
}
}

namespace AssetImport
{
bool ImageLoader::LoadFromFile(
    const std::filesystem::path& path,
    ImageData& outImage,
    std::string& outError)
{
    ResetResult(outImage, outError);
    if (path.empty())
    {
        outError = "Image path is empty.";
        return false;
    }

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        outError = "Failed to open image file: " + PathForError(path);
        return false;
    }

    const std::streampos endPosition = stream.tellg();
    if (endPosition <= 0)
    {
        outError = "Image file is empty: " + PathForError(path);
        return false;
    }

    const std::streamoff fileSize = endPosition;
    const auto encodedSize = static_cast<std::uintmax_t>(fileSize);
    if (encodedSize > static_cast<std::uintmax_t>(
            std::numeric_limits<int>::max()))
    {
        outError = "Image file is too large to decode: " + PathForError(path);
        return false;
    }

    std::vector<std::uint8_t> encodedData(
        static_cast<std::size_t>(encodedSize));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(
            reinterpret_cast<char*>(encodedData.data()),
            static_cast<std::streamsize>(encodedData.size())))
    {
        outError = "Failed to read complete image file: " + PathForError(path);
        return false;
    }

    if (!LoadFromMemory(encodedData, outImage, outError))
    {
        outError += " File: " + PathForError(path);
        return false;
    }

    return true;
}

bool ImageLoader::LoadFromMemory(
    const unsigned char* encodedData,
    std::size_t encodedSize,
    ImageData& outImage,
    std::string& outError)
{
    ResetResult(outImage, outError);
    if (encodedData == nullptr || encodedSize == 0)
    {
        outError = "Image memory data is empty.";
        return false;
    }

    if (encodedSize > static_cast<std::size_t>(
            std::numeric_limits<int>::max()))
    {
        outError = "Image memory data is too large to decode.";
        return false;
    }

    stbi_set_flip_vertically_on_load_thread(0);

    int width = 0;
    int height = 0;
    using StbiPixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;
    StbiPixels decodedPixels(
        stbi_load_from_memory(
            encodedData,
            static_cast<int>(encodedSize),
            &width,
            &height,
            nullptr,
            STBI_rgb_alpha),
        &stbi_image_free);
    if (!decodedPixels)
    {
        outError = DecodeFailureMessage();
        return false;
    }

    if (width <= 0 || height <= 0)
    {
        outError = "Image decode failed: invalid image dimensions.";
        return false;
    }

    const std::size_t decodedWidth = static_cast<std::size_t>(width);
    const std::size_t decodedHeight = static_cast<std::size_t>(height);
    if (decodedHeight >
        std::numeric_limits<std::size_t>::max() / decodedWidth)
    {
        outError = "Image decode failed: decoded dimensions are too large.";
        return false;
    }

    const std::size_t pixelCount = decodedWidth * decodedHeight;
    if (pixelCount >
        std::numeric_limits<std::size_t>::max() / ImageData::RgbaChannelCount)
    {
        outError = "Image decode failed: decoded pixel data is too large.";
        return false;
    }

    outImage.width = static_cast<std::uint32_t>(width);
    outImage.height = static_cast<std::uint32_t>(height);
    outImage.channels = ImageData::RgbaChannelCount;
    outImage.pixels.assign(
        decodedPixels.get(),
        decodedPixels.get() + pixelCount * ImageData::RgbaChannelCount);
    return true;
}

bool ImageLoader::LoadFromMemory(
    const std::vector<std::uint8_t>& encodedData,
    ImageData& outImage,
    std::string& outError)
{
    return LoadFromMemory(
        encodedData.data(),
        encodedData.size(),
        outImage,
        outError);
}
}
