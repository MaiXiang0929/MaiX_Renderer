// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>

#include "cyVector.h"

namespace EditorValueConstraints
{
constexpr float MinimumScale = 0.001f;
constexpr float MaximumScale = 1000.0f;
constexpr float MinimumLightRange = 0.01f;
constexpr float MaximumSpotConeDegrees = 89.0f;

inline void SanitizeScale(cy::Vec3f& scale)
{
    scale.x = std::clamp(std::abs(scale.x), MinimumScale, MaximumScale);
    scale.y = std::clamp(std::abs(scale.y), MinimumScale, MaximumScale);
    scale.z = std::clamp(std::abs(scale.z), MinimumScale, MaximumScale);
}

inline void SanitizeColor(cy::Vec3f& color)
{
    color.x = std::clamp(color.x, 0.0f, 1.0f);
    color.y = std::clamp(color.y, 0.0f, 1.0f);
    color.z = std::clamp(color.z, 0.0f, 1.0f);
}

inline void SanitizeDirection(cy::Vec3f& direction)
{
    if (direction.Length() <= 1.0e-5f)
    {
        direction = cy::Vec3f(0.0f, -1.0f, 0.0f);
        return;
    }
    direction.Normalize();
}

inline void SanitizeLightScalars(float& intensity, float& range)
{
    intensity = std::max(intensity, 0.0f);
    range = std::max(range, MinimumLightRange);
}

inline void SanitizeSpotConeDegrees(float& inner, float& outer)
{
    outer = std::clamp(outer, 1.0f, MaximumSpotConeDegrees);
    inner = std::clamp(inner, 0.0f, outer);
}
}
