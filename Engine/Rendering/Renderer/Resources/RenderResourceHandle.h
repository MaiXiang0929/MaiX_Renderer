// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <limits>

using RenderResourceId = std::uint32_t;
constexpr RenderResourceId InvalidRenderResourceId =
    std::numeric_limits<RenderResourceId>::max();
constexpr RenderResourceId DefaultSurfaceShaderId = 0;

struct MeshHandle
{
    RenderResourceId id = InvalidRenderResourceId;

    bool IsValid() const { return id != InvalidRenderResourceId; }
};

struct MaterialHandle
{
    RenderResourceId id = InvalidRenderResourceId;

    bool IsValid() const { return id != InvalidRenderResourceId; }
};
