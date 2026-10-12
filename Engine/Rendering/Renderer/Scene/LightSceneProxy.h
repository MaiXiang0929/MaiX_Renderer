// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <limits>

#include "cyVector.h"

using LightId = std::uint32_t;
constexpr LightId InvalidLightId = std::numeric_limits<LightId>::max();

enum class LightType
{
    Directional,
    Point,
    Spot
};

struct LightSceneProxy
{
    LightId id = InvalidLightId;
    LightType type = LightType::Point;
    cy::Vec3f position{ 0.0f, 0.0f, 0.0f };
    cy::Vec3f direction{ 0.0f, -1.0f, 0.0f };
    cy::Vec3f color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float range = 30.0f;
    float innerConeAngle = 20.0f * 3.14159265358979323846f / 180.0f;
    float outerConeAngle = 30.0f * 3.14159265358979323846f / 180.0f;
    bool castsShadow = true;
};
