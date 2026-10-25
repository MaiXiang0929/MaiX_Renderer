// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include "cyVector.h"

// Color coefficients are linear on the CPU. Only display color pickers and
// explicitly sRGB image inputs use the sRGB transfer function.
inline float SrgbToLinear(float value)
{
    value = std::max(value, 0.0f);
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}
inline float LinearToSrgb(float value)
{
    value = std::max(value, 0.0f);
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}
inline cy::Vec3f SrgbToLinear(const cy::Vec3f& value)
{ return {SrgbToLinear(value.x), SrgbToLinear(value.y), SrgbToLinear(value.z)}; }
inline cy::Vec3f LinearToSrgb(const cy::Vec3f& value)
{ return {LinearToSrgb(value.x), LinearToSrgb(value.y), LinearToSrgb(value.z)}; }

// Scene v1-v5 and built-in demo colors used pow(baseColor, 2.2) in the shader.
// This is migration, deliberately distinct from the exact sRGB conversion.
inline cy::Vec3f LegacyBaseColorToLinear(const cy::Vec3f& value)
{
    return {std::pow(std::max(value.x, 0.0f), 2.2f),
        std::pow(std::max(value.y, 0.0f), 2.2f), std::pow(std::max(value.z, 0.0f), 2.2f)};
}
