// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/SceneContent.h"
#include "Engine/Asset/Importer/ImportedModelData.h"
#include <atomic>
#include <map>

// Development safety limits, not a supported-world-size or hardware guarantee.
struct ScenePreparationLimits
{
    std::size_t cpuBytes=256u*1024u*1024u;
    std::size_t resourceBytes=64u*1024u*1024u;
    std::size_t sourceBytes=128u*1024u*1024u;
};
struct SceneCpuTexture
{
    AssetImport::ImageData image;
    AssetImport::TextureSource source;
};
struct SceneCpuModel
{
    std::filesystem::path source;
    std::vector<AssetImport::ImportedMeshData> meshes;
    cy::Vec3f size{0.0f};
    std::optional<double> sourceUnitMeters;
};
// Worker-owned values only: no Renderer, handles, World identity domain or GL objects.
struct SceneCpuContent
{
    SceneDocument document;
    SceneAssetRoots roots;
    std::map<std::pair<std::uint64_t,std::size_t>,SceneCpuTexture> textures;
    std::map<std::uint64_t,SceneCpuModel> models;
    std::size_t cpuBytes=0,gpuBytes=0;
    double preparationMs=0;
};
bool PrepareSceneCpuContent(const SceneDocument&,const SceneAssetRoots&,const ScenePreparationLimits&,
    const std::shared_ptr<std::atomic_bool>& cancel,std::unique_ptr<SceneCpuContent>&,std::string& error);

// Created, advanced, taken and destroyed exclusively with Renderer context current.
// Each operation is one texture/mesh/material/actor, plus a measured final CPU assembly.
class SceneContentUpload
{
public:
    SceneContentUpload();
    ~SceneContentUpload();
    SceneContentUpload(const SceneContentUpload&)=delete;
    SceneContentUpload& operator=(const SceneContentUpload&)=delete;
    bool Begin(std::unique_ptr<SceneCpuContent>,Renderer&,const SceneContentOptions&,std::string& error);
    bool Step(std::size_t operations,std::size_t bytes,double milliseconds,std::string& error);
    bool Complete() const;
    std::size_t CompletedOperations() const;
    std::size_t TotalOperations() const;
    double LastStepMs() const;
    std::unique_ptr<PreparedSceneContent> Take();
    SceneContentRetirement Abandon();
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
