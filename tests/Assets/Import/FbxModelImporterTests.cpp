// SPDX-License-Identifier: MIT
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "Assets/Import/FbxModelImporter.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[FbxModelImporterTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

std::filesystem::path MakeTemporaryFbxPath()
{
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        (std::wstring(L"MaiX_InvalidFbx_") +
         L"\x4E2D\x6587_" +
         std::to_wstring(suffix) +
         L".fbx");
}

void TestMissingFile()
{
    const std::filesystem::path path = MakeTemporaryFbxPath();
    const AssetImport::ModelImportResult result =
        AssetImport::FbxModelImporter::Import(path);
    Require(!result, "A missing FBX should fail to import.");
    Require(result.error.find("Unable to open model file") !=
            std::string::npos,
        "A missing FBX should report a file-open error.");
}

void TestInvalidFile()
{
    const std::filesystem::path path = MakeTemporaryFbxPath();
    {
        std::ofstream stream(path, std::ios::binary);
        stream << "This is not an FBX file.";
        Require(static_cast<bool>(stream),
            "The invalid FBX fixture should be written completely.");
    }

    const AssetImport::ModelImportResult result =
        AssetImport::FbxModelImporter::Import(path);
    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    Require(!result, "Invalid FBX bytes should fail to import.");
    Require(result.error.find("FBX parse failed") != std::string::npos,
        "Invalid FBX bytes should report a parser error.");
}

std::filesystem::path GetOptionalIntegrationPath()
{
#if defined(_WIN32)
    const wchar_t* value = _wgetenv(L"MAIX_TEST_FBX_PATH");
    return value != nullptr ? std::filesystem::path(value) :
        std::filesystem::path();
#else
    const char* value = std::getenv("MAIX_TEST_FBX_PATH");
    return value != nullptr ? std::filesystem::path(value) :
        std::filesystem::path();
#endif
}

void TestOptionalIntegrationAsset()
{
    const std::filesystem::path path = GetOptionalIntegrationPath();
    if (path.empty())
        return;

    const AssetImport::ModelImportResult result =
        AssetImport::FbxModelImporter::Import(path);
    if (!result)
        std::cerr << result.error << std::endl;
    Require(static_cast<bool>(result),
        "The optional integration FBX should import successfully.");
    Require(result.model.sourceMeshCount > 0,
        "The integration FBX should contain source meshes.");
    Require(!result.model.meshes.empty(),
        "The integration FBX should produce renderable sections.");
    std::size_t externalTextureCount = 0;
    for (const AssetImport::ImportedTextureData& texture :
        result.model.textures)
    {
        if (!texture.embedded)
            ++externalTextureCount;
    }
    Require(result.model.textureCandidates.size() >= externalTextureCount,
        "Discovered texture candidates should cover imported external textures.");
    Require(
        std::isfinite(result.model.boundsMin.x) &&
        std::isfinite(result.model.boundsMin.y) &&
        std::isfinite(result.model.boundsMin.z) &&
        std::isfinite(result.model.boundsMax.x) &&
        std::isfinite(result.model.boundsMax.y) &&
        std::isfinite(result.model.boundsMax.z),
        "Imported model bounds should be finite.");

    for (const AssetImport::ImportedMeshData& mesh : result.model.meshes)
    {
        Require(!mesh.vertices.empty(),
            "Each imported section should contain vertices.");
        Require(!mesh.indices.empty() && mesh.indices.size() % 3 == 0,
            "Each imported section should contain triangle indices.");
        Require(mesh.materialIndex < result.model.materials.size(),
            "Each imported section should reference a valid material.");
        for (std::uint32_t index : mesh.indices)
        {
            Require(index < mesh.vertices.size(),
                "Imported section indices should stay within the vertex array.");
        }
        if (std::abs(mesh.boundsCenter.x) > 10.0f ||
            std::abs(mesh.boundsCenter.y) > 10.0f ||
            std::abs(mesh.boundsCenter.z) > 10.0f ||
            mesh.boundsRadius > 10.0f)
        {
            std::cout
                << "  outlierSection=" << mesh.name
                << " center=(" << mesh.boundsCenter.x << ","
                << mesh.boundsCenter.y << "," << mesh.boundsCenter.z << ")"
                << " radius=" << mesh.boundsRadius
                << std::endl;
        }
    }
    for (const AssetImport::ImportedMaterialData& material :
        result.model.materials)
    {
        if (material.baseColorTexture ==
            AssetImport::InvalidImportedTextureIndex)
        {
            continue;
        }
        Require(material.baseColorTexture < result.model.textures.size(),
            "Imported material texture indices must remain in range.");
    }
    for (const AssetImport::ImportedTextureData& texture :
        result.model.textures)
    {
        Require(texture.image.IsValid(),
            "Every imported texture should contain valid RGBA8 data.");
    }

    std::cout
        << "Optional FBX: sourceMeshes=" << result.model.sourceMeshCount
        << " sections=" << result.model.meshes.size()
        << " materials=" << result.model.sourceMaterialCount
        << " textures=" << result.model.textures.size()
        << " candidates=" << result.model.textureCandidates.size()
        << " skins=" << result.model.skinDeformerCount
        << " animations=" << result.model.animationStackCount
        << " boundsMin=(" << result.model.boundsMin.x << ","
        << result.model.boundsMin.y << "," << result.model.boundsMin.z << ")"
        << " boundsMax=(" << result.model.boundsMax.x << ","
        << result.model.boundsMax.y << "," << result.model.boundsMax.z << ")"
        << " warnings=" << result.model.warnings.size()
        << std::endl;
    for (const std::string& warning : result.model.warnings)
        std::cout << "  warning: " << warning << std::endl;
}
}

int main()
{
    TestMissingFile();
    TestInvalidFile();
    TestOptionalIntegrationAsset();

    std::cout << "FBX model importer tests passed." << std::endl;
    return EXIT_SUCCESS;
}
