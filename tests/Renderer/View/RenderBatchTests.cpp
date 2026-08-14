// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>
#include <vector>

#include "Renderer/View/RenderBatch.h"
#include "Renderer/View/RenderItem.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[RenderBatchTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

RenderItem MakeItem(
    RenderResourceId shaderId,
    RenderResourceId materialId,
    RenderResourceId meshId)
{
    RenderItem item;
    item.shaderId = shaderId;
    item.materialId = materialId;
    item.meshId = meshId;
    return item;
}

void TestEmptyItems()
{
    Require(BuildOpaqueRenderBatches({}).empty(),
        "An empty item list should not produce batches.");
}

void TestContiguousResourceGrouping()
{
    const std::vector<RenderItem> items = {
        MakeItem(0, 1, 2),
        MakeItem(0, 1, 2),
        MakeItem(0, 1, 3),
        MakeItem(0, 2, 3),
        MakeItem(1, 2, 3),
        MakeItem(1, 2, 3)
    };
    const std::vector<OpaqueRenderBatch> batches =
        BuildOpaqueRenderBatches(items);

    Require(batches.size() == 4,
        "Every complete resource-key transition should start a batch.");
    Require(batches[0].firstItem == 0 && batches[0].itemCount == 2,
        "The first identical resource pair should be grouped.");
    Require(batches[1].firstItem == 2 && batches[1].itemCount == 1,
        "A mesh transition should split the batch.");
    Require(batches[2].firstItem == 3 && batches[2].itemCount == 1,
        "A material transition should split the batch.");
    Require(batches[3].firstItem == 4 && batches[3].itemCount == 2,
        "The final shader group should preserve both items.");
    Require(ShouldUseInstancing(batches[0]) &&
        !ShouldUseInstancing(batches[1]) &&
        ShouldUseInstancing(batches[3]),
        "Only batches with multiple items should use instancing.");
}

void TestNonContiguousItemsDoNotMerge()
{
    const std::vector<RenderItem> items = {
        MakeItem(0, 1, 2),
        MakeItem(0, 2, 2),
        MakeItem(0, 1, 2)
    };
    const std::vector<OpaqueRenderBatch> batches =
        BuildOpaqueRenderBatches(items);
    Require(batches.size() == 3,
        "Batching must not reorder or merge non-contiguous items.");
}
}

int main()
{
    TestEmptyItems();
    TestContiguousResourceGrouping();
    TestNonContiguousItemsDoNotMerge();
    std::cout << "Render batch tests passed." << std::endl;
    return EXIT_SUCCESS;
}
