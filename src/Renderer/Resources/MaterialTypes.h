// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>

#include "Renderer/Resources/TextureTypes.h"

/// Controls which surface queue and fixed-function blend state a material uses.
enum class BlendMode
{
    Opaque,
    AlphaBlend
};

enum class ShadingModel
{
    PBR,
    Toon
};

constexpr float MinimumOutlineThickness = 0.0f;
constexpr float MaximumOutlineThickness = 0.2f;

constexpr float ClampOutlineThickness(float thickness)
{
    return thickness < MinimumOutlineThickness
        ? MinimumOutlineThickness
        : thickness > MaximumOutlineThickness
            ? MaximumOutlineThickness
            : thickness;
}

enum class MaterialTextureSlot : std::uint8_t
{
    BaseColor,
    Normal,
    OcclusionRoughnessMetallic,
    Displacement,
    LegacySpecular,
    Count
};

constexpr std::size_t MaterialTextureSlotCount =
    static_cast<std::size_t>(MaterialTextureSlot::Count);

constexpr std::size_t ToIndex(MaterialTextureSlot slot)
{
    return static_cast<std::size_t>(slot);
}

constexpr unsigned int GetMaterialTextureUnitOffset(
    MaterialTextureSlot slot)
{
    switch (slot)
    {
    case MaterialTextureSlot::BaseColor: return 0;
    case MaterialTextureSlot::OcclusionRoughnessMetallic:
    case MaterialTextureSlot::LegacySpecular: return 1;
    case MaterialTextureSlot::Normal: return 2;
    case MaterialTextureSlot::Displacement: return 3;
    case MaterialTextureSlot::Count: break;
    }
    return 0;
}

constexpr TextureColorSpace GetRequiredMaterialTextureColorSpace(
    MaterialTextureSlot slot)
{
    return slot == MaterialTextureSlot::BaseColor
        ? TextureColorSpace::SRGB
        : TextureColorSpace::Linear;
}
