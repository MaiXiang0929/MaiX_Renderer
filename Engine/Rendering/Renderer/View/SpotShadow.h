// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"
#include "cyMatrix.h"
#include <cmath>

// Square shadow map: outerConeAngle is a half angle, range is the far plane.
// The nearest 0.1% of range is clipped to keep the perspective projection finite.
inline bool BuildSpotShadow(const LightSceneProxy& light, cy::Matrix4f& lightVP, cy::Vec2f& biasParams)
{
    const auto finite = [](const cy::Vec3f& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    const float directionLengthSquared = light.direction.LengthSquared();
    constexpr float HalfPi = 1.57079632679489661923f;
    if (!finite(light.position) || !finite(light.direction) ||
        !std::isfinite(directionLengthSquared) || directionLengthSquared < 1e-12f ||
        !std::isfinite(light.range) || light.range <= 0 ||
        !std::isfinite(light.outerConeAngle) || light.outerConeAngle <= 0 ||
        light.outerConeAngle >= HalfPi)
        return false;

    const auto direction = light.direction / std::sqrt(directionLengthSquared);
    const auto up = std::abs(direction.y) > 0.99f ? cy::Vec3f(0, 0, 1) : cy::Vec3f(0, 1, 0);
    const auto view = cy::Matrix4f::View(cy::Vec3f(0.0f), direction, up) *
        cy::Matrix4f::Translation(-light.position);
    const float nearPlane = light.range * 0.001f;
    const auto candidate = cy::Matrix4f::Perspective(2 * light.outerConeAngle, 1,
        nearPlane, light.range) * view;
    for (float value : candidate.cell)
        if (!std::isfinite(value)) return false;
    lightVP = candidate;
    biasParams = {nearPlane / 0.999f, 2 * std::tan(light.outerConeAngle)};
    return true;
}
