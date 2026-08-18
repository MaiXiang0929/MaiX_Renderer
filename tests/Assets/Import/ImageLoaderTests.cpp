// SPDX-License-Identifier: MIT
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "Assets/Import/ImageLoader.h"

namespace
{
const std::vector<std::uint8_t> TestPng = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x99, 0x81, 0xB6, 0x27,
    0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00,
    0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00, 0x00, 0x04, 0x67,
    0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC,
    0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59,
    0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3,
    0x01, 0xC7, 0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x12,
    0x49, 0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0xF8, 0xCF,
    0xC0, 0xF0, 0x9F, 0x81, 0x81, 0xE1, 0xFF, 0x7F, 0x00,
    0x11, 0xF8, 0x03, 0xFD, 0x6A, 0x5C, 0xEF, 0xC9, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42,
    0x60, 0x82,
};

void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[ImageLoaderTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

std::filesystem::path MakeUnicodeTemporaryPath()
{
    const auto uniqueSuffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        (std::wstring(L"MaiX_ImageLoader_") +
         L"\x4E2D\x6587_" +
         std::to_wstring(uniqueSuffix) +
         L".png");
}

void ValidateTestImage(const AssetImport::ImageData& image)
{
    Require(image.IsValid(), "Decoded image should satisfy the RGBA8 contract.");
    Require(image.width == 1 && image.height == 2,
        "Decoded image dimensions should match the source PNG.");
    Require(image.pixels.size() == 8,
        "A 1x2 RGBA8 image should contain eight bytes.");
    Require(
        image.pixels[0] == 255 && image.pixels[1] == 0 &&
        image.pixels[2] == 0 && image.pixels[3] == 255,
        "The top source pixel should remain red.");
    Require(
        image.pixels[4] == 0 && image.pixels[5] == 0 &&
        image.pixels[6] == 255 && image.pixels[7] == 255,
        "The bottom source pixel should remain blue (no vertical flip).");
}

void TestMemoryDecode()
{
    AssetImport::ImageData image;
    std::string error;
    Require(AssetImport::ImageLoader::LoadFromMemory(TestPng, image, error),
        "A valid in-memory PNG should decode.");
    Require(error.empty(), "Successful memory decode should not return an error.");
    ValidateTestImage(image);
}

void TestInvalidMemory()
{
    const std::vector<std::uint8_t> invalidData = {0x01, 0x02, 0x03};
    AssetImport::ImageData image;
    std::string error;
    Require(!AssetImport::ImageLoader::LoadFromMemory(
            invalidData, image, error),
        "Invalid encoded bytes should fail to decode.");
    Require(!error.empty(), "Invalid encoded bytes should return a clear error.");
    Require(!image.IsValid(), "A failed memory decode should clear image output.");
}

void TestMissingPath()
{
    AssetImport::ImageData image;
    std::string error;
    const std::filesystem::path missingPath = MakeUnicodeTemporaryPath();
    Require(!AssetImport::ImageLoader::LoadFromFile(missingPath, image, error),
        "A missing image path should fail to load.");
    Require(error.find("Failed to open image file") != std::string::npos,
        "A missing path should identify the file-open failure.");
}

void TestUnicodePathDecode()
{
    const std::filesystem::path temporaryPath = MakeUnicodeTemporaryPath();
    std::ofstream stream(temporaryPath, std::ios::binary);
    const bool opened = static_cast<bool>(stream);
    stream.write(
        reinterpret_cast<const char*>(TestPng.data()),
        static_cast<std::streamsize>(TestPng.size()));
    stream.close();
    const bool wroteCompleteFile = static_cast<bool>(stream);

    AssetImport::ImageData image;
    std::string error;
    const bool loaded = AssetImport::ImageLoader::LoadFromFile(
        temporaryPath, image, error);
    std::error_code removeError;
    std::filesystem::remove(temporaryPath, removeError);

    Require(opened, "The Unicode temporary image path should be writable.");
    Require(wroteCompleteFile, "The complete temporary PNG should be written.");
    Require(loaded,
        "A PNG at a Unicode filesystem path should decode.");
    Require(error.empty(), "Successful path decode should not return an error.");
    ValidateTestImage(image);
}
}

int main()
{
    TestMemoryDecode();
    TestInvalidMemory();
    TestMissingPath();
    TestUnicodePathDecode();

    std::cout << "Image loader tests passed." << std::endl;
    return EXIT_SUCCESS;
}
