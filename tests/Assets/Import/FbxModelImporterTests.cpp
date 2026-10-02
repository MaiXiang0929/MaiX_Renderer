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

void TestUnitConversionAndNodeTransforms()
{
    // The same triangle is authored once in meters and once in centimeters.
    // A scaled parent and translated child expose both lost and doubled transforms.
    for (double unitsPerMeter : {1.0, 100.0})
    {
        const std::filesystem::path path = MakeTemporaryFbxPath();
        {
            std::ofstream stream(path);
            stream << "; FBX 7.4.0 project fixture\n"
                "FBXHeaderExtension: { FBXVersion: 7400 }\n"
                "GlobalSettings: { Properties70: {\n"
                "P: \"UpAxis\", \"int\", \"Integer\", \"\",1\n"
                "P: \"UpAxisSign\", \"int\", \"Integer\", \"\",1\n"
                "P: \"FrontAxis\", \"int\", \"Integer\", \"\",2\n"
                "P: \"FrontAxisSign\", \"int\", \"Integer\", \"\",1\n"
                "P: \"CoordAxis\", \"int\", \"Integer\", \"\",0\n"
                "P: \"CoordAxisSign\", \"int\", \"Integer\", \"\",1\n"
                "P: \"UnitScaleFactor\", \"double\", \"Number\", \"\","
                << 100.0 / unitsPerMeter << "\n"
                "P: \"OriginalUnitScaleFactor\", \"double\", \"Number\", \"\",1\n} }\n"
                "Objects: {\n"
                "Geometry: 100, \"Geometry::Triangle\", \"Mesh\" {\n"
                "Vertices: *9 { a: 0,0,0," << unitsPerMeter << ",0,0,0," << unitsPerMeter << ",0 }\n"
                "PolygonVertexIndex: *3 { a: 0,1,-3 }\n}\n"
                "Model: 200, \"Model::Child\", \"Mesh\" { Properties70: {\n"
                "P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\","
                << unitsPerMeter << ",0,0\n} }\n"
                "Model: 300, \"Model::Parent\", \"Null\" { Properties70: {\n"
                "P: \"Lcl Scaling\", \"Lcl Scaling\", \"\", \"A\",2,2,2\n} }\n}\n"
                "Connections: {\nC: \"OO\",100,200\nC: \"OO\",200,300\nC: \"OO\",300,0\n}\n";
            Require(static_cast<bool>(stream), "Unit fixture must be written completely.");
        }
        const AssetImport::ModelImportResult result = AssetImport::FbxModelImporter::Import(path);
        std::error_code removeError;
        std::filesystem::remove(path, removeError);
        if (!result) std::cerr << result.error << std::endl;
        Require(static_cast<bool>(result), "Both meter and centimeter fixtures should import.");
        Require(result.model.sourceUnitMeters.has_value() &&
            std::abs(*result.model.sourceUnitMeters - 1.0 / unitsPerMeter) < 1.0e-6,
            "Diagnostics must retain original source units.");
        std::cout << "Unit fixture " << unitsPerMeter << " units/m: boundsMin=("
            << result.model.boundsMin.x << "," << result.model.boundsMin.y << "," << result.model.boundsMin.z
            << ") boundsMax=(" << result.model.boundsMax.x << "," << result.model.boundsMax.y
            << "," << result.model.boundsMax.z << ")" << std::endl;
        Require(std::abs(result.model.boundsMin.x - 2.0f) < 1.0e-4f &&
            std::abs(result.model.boundsMax.x - 4.0f) < 1.0e-4f &&
            std::abs(result.model.boundsMax.y - 2.0f) < 1.0e-4f,
            "Units, hierarchy scale, and node translation must each be applied exactly once.");
        Require(result.model.meshes.size() == 1 && result.model.meshes.front().indices.size() == 3,
            "Unit conversion must preserve triangle topology.");
    }
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
    TestUnitConversionAndNodeTransforms();
    TestOptionalIntegrationAsset();

    std::cout << "FBX model importer tests passed." << std::endl;
    return EXIT_SUCCESS;
}
