// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <limits>
#include <cmath>

#include "cyVector.h"

using LightId = std::uint32_t;
constexpr LightId InvalidLightId = std::numeric_limits<LightId>::max();

enum class LightType
{
    Directional,
    Point,
    Spot
};

enum class LightFalloff { Legacy, InverseSquare };
constexpr float MinimumPhysicalLightDistance = 0.01f; // Meters; regularizes the point source singularity.
constexpr float MaximumPhysicalLightIntensity = 1000000.0f;
constexpr float MaximumPhysicalLightRange = 100000.0f;

inline bool SupportsShadowMap(LightType type)
{
    return type == LightType::Directional || type == LightType::Spot || type == LightType::Point;
}

struct LightSceneProxy
{
    LightId id = InvalidLightId;
    LightType type = LightType::Point;
    cy::Vec3f position{ 0.0f, 0.0f, 0.0f };
    cy::Vec3f direction{ 0.0f, -1.0f, 0.0f };
    cy::Vec3f color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    // Legacy intensity is unitless. InverseSquare: local lights in candela,
    // directional lights in lux. One world unit is one meter.
    LightFalloff falloff = LightFalloff::Legacy;
    float range = 30.0f;
    float innerConeAngle = 20.0f * 3.14159265358979323846f / 180.0f;
    float outerConeAngle = 30.0f * 3.14159265358979323846f / 180.0f;
    bool castsShadow = true;
    bool visible = true;
    float shadowBias = 0.5f; // In world texels at the receiver.
    float shadowNormalBias = 2.0f;
    unsigned cascadeCount = 4;
    float shadowDistance = 50.0f;
    float splitLambda = 0.5f;
};

inline bool IsValidLightPhotometry(const LightSceneProxy& light)
{
    return (light.falloff == LightFalloff::Legacy || light.falloff == LightFalloff::InverseSquare) &&
        std::isfinite(light.intensity) && light.intensity >= 0 &&
        std::isfinite(light.range) && light.range > 0 &&
        std::isfinite(light.color.x) && std::isfinite(light.color.y) && std::isfinite(light.color.z) &&
        light.color.x >= 0 && light.color.y >= 0 && light.color.z >= 0 &&
        (light.falloff == LightFalloff::Legacy ||
            (light.intensity <= MaximumPhysicalLightIntensity && light.color.Max() <= 1 &&
                light.range >= MinimumPhysicalLightDistance && light.range <= MaximumPhysicalLightRange));
}

inline bool IsValidLightShadowSettings(const LightSceneProxy& light)
{
    return std::isfinite(light.shadowBias) && light.shadowBias >= 0 && light.shadowBias <= 10 &&
        std::isfinite(light.shadowNormalBias) && light.shadowNormalBias >= 0 && light.shadowNormalBias <= 10 &&
        (light.cascadeCount == 1 || light.cascadeCount == 2 || light.cascadeCount == 4) &&
        std::isfinite(light.shadowDistance) && light.shadowDistance >= 0.2f && light.shadowDistance <= 100000 &&
        std::isfinite(light.splitLambda) && light.splitLambda >= 0 && light.splitLambda <= 1;
}
