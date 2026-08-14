// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <vector>

#include "Renderer/Resources/RenderResourceHandle.h"

struct RenderItem;

struct OpaqueRenderBatch
{
    std::size_t firstItem = 0;
    std::size_t itemCount = 0;
    RenderResourceId shaderId = InvalidRenderResourceId;
    RenderResourceId materialId = InvalidRenderResourceId;
    RenderResourceId meshId = InvalidRenderResourceId;
};

std::vector<OpaqueRenderBatch> BuildOpaqueRenderBatches(
    const std::vector<RenderItem>& items);

inline bool ShouldUseInstancing(const OpaqueRenderBatch& batch)
{
    return batch.itemCount > 1;
}
