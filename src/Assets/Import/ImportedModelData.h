// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Assets/Import/ImageData.h"
#include "Renderer/Resources/Mesh.h"

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
};

struct ImportedMaterialData
{
    std::string name;
    cy::Vec3f baseColor{ 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 0.5f;
    float opacity = 1.0f;
    std::uint32_t baseColorTexture = InvalidImportedTextureIndex;
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
    std::size_t sourceMeshCount = 0;
    std::size_t sourceMaterialCount = 0;
    std::size_t skinDeformerCount = 0;
    std::size_t animationStackCount = 0;
    std::vector<std::string> warnings;
};

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
