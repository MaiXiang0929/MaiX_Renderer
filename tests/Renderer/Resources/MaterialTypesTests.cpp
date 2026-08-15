// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>

#include "Renderer/Resources/MaterialTypes.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[MaterialTypesTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

void TestTextureSlotIndices()
{
    Require(MaterialTextureSlotCount == 5,
        "The PBR material contract should expose five semantic slots.");
    Require(ToIndex(MaterialTextureSlot::BaseColor) == 0 &&
        ToIndex(MaterialTextureSlot::LegacySpecular) == 4,
        "Material texture slots should retain stable array indices.");
}

void TestShadingModels()
{
    Require(static_cast<int>(ShadingModel::PBR) == 0 &&
        static_cast<int>(ShadingModel::Toon) == 1,
        "Shading model values must match the GLSL material contract.");
}

void TestOutlineThicknessClamp()
{
    Require(ClampOutlineThickness(-1.0f) == MinimumOutlineThickness,
        "Outline thickness should clamp negative values.");
    Require(ClampOutlineThickness(0.05f) == 0.05f,
        "Outline thickness should retain values inside the supported range.");
    Require(ClampOutlineThickness(1.0f) == MaximumOutlineThickness,
        "Outline thickness should clamp values above the supported range.");
}

void TestFixedTextureUnits()
{
    Require(GetMaterialTextureUnitOffset(MaterialTextureSlot::BaseColor) == 0,
        "Base Color should use the first material texture unit.");
    Require(GetMaterialTextureUnitOffset(
        MaterialTextureSlot::OcclusionRoughnessMetallic) == 1,
        "ORM should use the packed-data texture unit.");
    Require(GetMaterialTextureUnitOffset(
        MaterialTextureSlot::LegacySpecular) == 1,
        "Legacy Specular should share the mutually-exclusive ORM unit.");
    Require(GetMaterialTextureUnitOffset(MaterialTextureSlot::Normal) == 2,
        "Normal should use the third material texture unit.");
    Require(GetMaterialTextureUnitOffset(MaterialTextureSlot::Displacement) == 3,
        "Displacement should use the fourth material texture unit.");
}

void TestTextureColorSpaces()
{
    Require(GetRequiredMaterialTextureColorSpace(
        MaterialTextureSlot::BaseColor) == TextureColorSpace::SRGB,
        "Base Color textures must use hardware sRGB decoding.");
    Require(GetRequiredMaterialTextureColorSpace(
        MaterialTextureSlot::Normal) == TextureColorSpace::Linear &&
        GetRequiredMaterialTextureColorSpace(
            MaterialTextureSlot::OcclusionRoughnessMetallic) ==
            TextureColorSpace::Linear &&
        GetRequiredMaterialTextureColorSpace(
            MaterialTextureSlot::Displacement) == TextureColorSpace::Linear,
        "Normal, ORM, and displacement textures must remain linear data.");
}
}

int main()
{
    TestTextureSlotIndices();
    TestShadingModels();
    TestOutlineThicknessClamp();
    TestFixedTextureUnits();
    TestTextureColorSpaces();
    std::cout << "Material type tests passed." << std::endl;
    return EXIT_SUCCESS;
}
