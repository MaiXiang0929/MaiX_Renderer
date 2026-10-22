// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Engine/Core/Math/Transform.h"
#include "Engine/Runtime/Scene/World.h"
#include "Engine/Rendering/Renderer/Scene/PrimitiveSceneProxy.h"

struct ModelId
{
    std::uint64_t value = 0;
    bool IsValid() const { return value != 0; }
    bool operator==(ModelId other) const { return value == other.value; }
    bool operator!=(ModelId other) const { return value != other.value; }
};

struct SceneModelSection
{
    PrimitiveId primitiveId = InvalidPrimitiveId;
    cy::Matrix4f localTransform = cy::Matrix4f::Identity();
    PrimitiveBounds localBounds;
    // 记录实际材质绑定；替换由 Renderer 事务接口提交后同步此句柄，不拥有 GPU 对象。
    MaterialHandle material;
    std::string name;
};

struct SceneModel
{
    ActorId actor;
    bool visible = true; // Derived from World, including hidden ancestors.
    ModelId id;
    std::string name;
    Transform transform;
    std::vector<SceneModelSection> sections;
    // Renderer owns the GPU objects; the model records handles for ordered cleanup.
    std::vector<MeshHandle> meshes;
    std::vector<MaterialHandle> materials;
    // Import diagnostics are values: no ownership of importer or GPU objects.
    std::optional<cy::Vec3f> importedSizeMeters;
    std::optional<double> sourceUnitMeters;
    // Reload provenance, independent of runtime handles and display names.
    std::filesystem::path sourcePath;
    std::vector<std::string> sourceMeshNames;
    float sourceCubeEdge = 0.0f;

    bool IsValid() const { return !sections.empty(); }
    PrimitiveBounds GetWorldBounds() const;
    std::vector<MaterialHandle> GetUsedMaterials() const;
};

SceneModel* FindSceneModel(std::vector<SceneModel>& models, ModelId id);
const SceneModel* FindSceneModel(
    const std::vector<SceneModel>& models, ModelId id);
std::string MakeUniqueModelName(
    const std::vector<SceneModel>& models,
    const std::string& requestedName);
PrimitiveBounds GetSceneWorldBounds(const std::vector<SceneModel>& models);

void ApplySceneModelTransform(
    SceneModel& model,
    class Renderer& renderer);
