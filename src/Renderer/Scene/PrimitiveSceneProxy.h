// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <limits>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Renderer/Resources/RenderResourceHandle.h"
#include "Renderer/Resources/MaterialTypes.h"

class Material;
class Mesh;

using PrimitiveId = std::uint32_t;
constexpr PrimitiveId InvalidPrimitiveId =
    std::numeric_limits<PrimitiveId>::max();

struct PrimitiveBounds
{
    cy::Vec3f center{ 0.0f, 0.0f, 0.0f };
    float radius = 0.0f;
};

PrimitiveBounds TransformBounds(
    const PrimitiveBounds& localBounds,
    const cy::Matrix4f& localToWorld);

struct PrimitiveSceneProxy
{
    PrimitiveId id = InvalidPrimitiveId;
    Mesh* mesh = nullptr;
    const Material* material = nullptr;
    RenderResourceId shaderId = DefaultSurfaceShaderId;
    RenderResourceId materialId = InvalidRenderResourceId;
    RenderResourceId meshId = InvalidRenderResourceId;
    cy::Matrix4f localToWorld = cy::Matrix4f::Identity();
    PrimitiveBounds localBounds;
    bool visible = true;
    bool castsShadow = true;
    BlendMode blendMode = BlendMode::Opaque;
};
