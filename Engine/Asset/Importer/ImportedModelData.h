// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Engine/Asset/Importer/ImageData.h"
#include "Engine/Asset/Importer/TextureSource.h"
#include "Engine/Asset/Geometry/Vertex.h"

namespace AssetImport
{
constexpr std::uint32_t InvalidImportedTextureIndex =
    std::numeric_limits<std::uint32_t>::max();

struct ImportedTextureData
{
    std::string name;
    std::filesystem::path sourcePath;
    ImageData image;
    bool embedded = false;
    std::uint64_t sourceId = 0; // Stable ufbx element identity + 1, independent of decoded array order.
    std::optional<TextureSource> reconstruction;
};

struct ImportedMaterialData
{
    std::string name;
    // Numeric FBX reflectance coefficient, treated as linear (not an image texel).
    cy::Vec3f baseColor{ 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ambientOcclusion = 1.0f;
    float opacity = 1.0f;
    std::uint32_t baseColorTexture = InvalidImportedTextureIndex;
    std::uint32_t normalTexture = InvalidImportedTextureIndex;
    std::uint32_t ormTexture = InvalidImportedTextureIndex;
};

struct ImportedMeshData
{
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t materialIndex = 0;
    cy::Matrix4f localToWorld = cy::Matrix4f::Identity();
    cy::Vec3f boundsCenter{ 0.0f, 0.0f, 0.0f };
    float boundsRadius = 0.0f;
};

struct ImportedModelData
{
    std::filesystem::path sourcePath;
    std::string name;
    std::vector<ImportedMeshData> meshes;
    std::vector<ImportedMaterialData> materials;
    std::vector<ImportedTextureData> textures;
    std::vector<std::filesystem::path> textureCandidates;
    cy::Vec3f boundsMin{ 0.0f, 0.0f, 0.0f };
    cy::Vec3f boundsMax{ 0.0f, 0.0f, 0.0f };
    // Reported source unit, not an extra multiplier on the converted vertices.
    std::optional<double> sourceUnitMeters;
    std::size_t sourceMeshCount = 0;
    std::size_t sourceMaterialCount = 0;
    std::size_t skinDeformerCount = 0;
    std::size_t animationStackCount = 0;
    std::vector<std::string> warnings;
};

inline const ImportedTextureData* FindEmbeddedTexture(const ImportedModelData& model,std::uint64_t identity)
{
    if (!identity) return nullptr;
    for (const auto& texture:model.textures)
        if (texture.embedded&&texture.sourceId==identity) return &texture;
    return nullptr;
}

struct ModelImportResult
{
    ImportedModelData model;
    std::string error;

    explicit operator bool() const
    {
        return error.empty() && !model.meshes.empty();
    }
};
}
