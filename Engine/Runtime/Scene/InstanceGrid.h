// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "cyVector.h"

std::vector<cy::Vec3f> BuildInstanceGridOffsets(
    std::uint32_t gridSize,
    float spacing);

float CalculateInstanceGridSceneRadius(
    std::uint32_t gridSize,
    float spacing,
    float instanceRadius);
