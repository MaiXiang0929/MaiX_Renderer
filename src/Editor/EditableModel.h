// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "Core/Transform.h"
#include "Renderer/Scene/PrimitiveSceneProxy.h"

struct EditableModelSection
{
    PrimitiveId primitiveId = InvalidPrimitiveId;
    cy::Matrix4f localTransform = cy::Matrix4f::Identity();
    PrimitiveBounds localBounds;
};

struct EditableModel
{
    std::string name;
    Transform transform;
    std::vector<EditableModelSection> sections;

    bool IsValid() const { return !sections.empty(); }
    PrimitiveBounds GetWorldBounds() const;
};

void ApplyEditableModelTransform(
    EditableModel& model,
    class Renderer& renderer);
