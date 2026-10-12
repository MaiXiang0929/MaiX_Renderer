// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/InstanceGrid.h"

#include <cmath>

std::vector<cy::Vec3f> BuildInstanceGridOffsets(
    std::uint32_t gridSize,
    float spacing)
{
    std::vector<cy::Vec3f> offsets;
    if (gridSize == 0 || spacing <= 0.0f)
        return offsets;

    offsets.reserve(static_cast<std::size_t>(gridSize) * gridSize);
    const float halfSpan = static_cast<float>(gridSize - 1) * spacing * 0.5f;
    for (std::uint32_t row = 0; row < gridSize; ++row)
    {
        for (std::uint32_t column = 0; column < gridSize; ++column)
        {
            offsets.emplace_back(
                static_cast<float>(column) * spacing - halfSpan,
                static_cast<float>(row) * spacing - halfSpan,
                0.0f);
        }
    }
    return offsets;
}

float CalculateInstanceGridSceneRadius(
    std::uint32_t gridSize,
    float spacing,
    float instanceRadius)
{
    if (gridSize == 0 || spacing <= 0.0f || instanceRadius < 0.0f)
        return 0.0f;

    const float halfSpan = static_cast<float>(gridSize - 1) * spacing * 0.5f;
    return std::sqrt(2.0f) * halfSpan + instanceRadius;
}
