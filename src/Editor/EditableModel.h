// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Transform.h"
#include "Renderer/Scene/PrimitiveSceneProxy.h"

struct ModelId
{
    std::uint64_t value = 0;
    bool IsValid() const { return value != 0; }
    bool operator==(ModelId other) const { return value == other.value; }
    bool operator!=(ModelId other) const { return value != other.value; }
};

struct EditableModelSection
{
    PrimitiveId primitiveId = InvalidPrimitiveId;
    cy::Matrix4f localTransform = cy::Matrix4f::Identity();
    PrimitiveBounds localBounds;
};

struct EditableModel
{
    ModelId id;
    std::string name;
    Transform transform;
    std::vector<EditableModelSection> sections;
    // Renderer owns the GPU objects; the model records handles for ordered cleanup.
    std::vector<MeshHandle> meshes;
    std::vector<MaterialHandle> materials;

    bool IsValid() const { return !sections.empty(); }
    PrimitiveBounds GetWorldBounds() const;
};

EditableModel* FindEditableModel(std::vector<EditableModel>& models, ModelId id);
const EditableModel* FindEditableModel(
    const std::vector<EditableModel>& models, ModelId id);
std::string MakeUniqueModelName(
    const std::vector<EditableModel>& models,
    const std::string& requestedName);
PrimitiveBounds GetSceneWorldBounds(const std::vector<EditableModel>& models);

void ApplyEditableModelTransform(
    EditableModel& model,
    class Renderer& renderer);
