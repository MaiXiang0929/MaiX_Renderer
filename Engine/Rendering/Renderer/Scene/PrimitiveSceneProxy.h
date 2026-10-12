// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <limits>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Engine/Rendering/Renderer/Resources/RenderResourceHandle.h"
#include "Engine/Rendering/Material/MaterialTypes.h"

class Material;
class Mesh;

using PrimitiveId = std::uint32_t;
constexpr PrimitiveId InvalidPrimitiveId =
    std::numeric_limits<PrimitiveId>::max();

#include "Engine/Core/Math/Bounds.h"

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
