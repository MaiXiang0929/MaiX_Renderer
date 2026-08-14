// SPDX-License-Identifier: MIT
#include "RenderBatch.h"

#include "Renderer/View/RenderItem.h"

namespace
{
bool HasSameResources(const RenderItem& left, const RenderItem& right)
{
    return left.shaderId == right.shaderId &&
        left.materialId == right.materialId &&
        left.meshId == right.meshId;
}
}

std::vector<OpaqueRenderBatch> BuildOpaqueRenderBatches(
    const std::vector<RenderItem>& items)
{
    std::vector<OpaqueRenderBatch> batches;
    if (items.empty())
        return batches;

    batches.reserve(items.size());
    std::size_t firstItem = 0;
    while (firstItem < items.size())
    {
        std::size_t endItem = firstItem + 1;
        while (endItem < items.size() &&
            HasSameResources(items[firstItem], items[endItem]))
            ++endItem;

        const RenderItem& first = items[firstItem];
        batches.push_back({
            firstItem,
            endItem - firstItem,
            first.shaderId,
            first.materialId,
            first.meshId
        });
        firstItem = endItem;
    }
    return batches;
}
