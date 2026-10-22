// SPDX-License-Identifier: MIT
#pragma once
#include <bitset>
#include "cyVector.h"
#include "Engine/Rendering/Material/MaterialTypes.h"

struct MaterialProperties
{
    cy::Vec3f baseColor{ 1.0f, 1.0f, 1.0f };
    cy::Vec3f specularColor{ 1.0f, 1.0f, 1.0f };
    float shininess = 64.0f;
    float environmentReflectivity = 0.5f;

    // Cook-Torrance metallic-roughness inputs consumed by the forward shader.
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ambientOcclusion = 1.0f;
    float normalScale = 1.0f;
    // 透明 Pass 使用的整体不透明度；管线分类仍由 Primitive 单独决定。
    float opacity = 1.0f;
    ShadingModel shadingModel = ShadingModel::PBR;
    float toonThreshold = 0.5f;
    float toonShadowStrength = 0.65f;
    cy::Vec3f toonShadowColor{ 0.18f, 0.20f, 0.28f };
    float rimLightStrength = 0.0f;
    cy::Vec3f rimLightColor{ 1.0f, 1.0f, 1.0f };
    bool faceShadowEnabled = false;
    cy::Vec3f faceForwardLocal{ 0.0f, 0.0f, 1.0f };
    cy::Vec3f faceRightLocal{ 1.0f, 0.0f, 0.0f };
    float faceShadowSoftness = 0.02f;
    bool faceShadowMirrorX = false;
    bool outlineEnabled = false;
    float outlineThickness = 0.025f;
    cy::Vec3f outlineColor{ 0.02f, 0.02f, 0.02f };
};

enum class MaterialParameter : std::uint8_t
{
    BaseColor,
    SpecularColor,
    Shininess,
    EnvironmentReflectivity,
    Metallic,
    Roughness,
    AmbientOcclusion,
    NormalScale,
    Opacity,
    ToonThreshold,
    ToonShadowStrength,
    ToonShadowColor,
    RimLightStrength,
    RimLightColor,
    FaceShadowEnabled,
    FaceFrame,
    FaceShadowSoftness,
    FaceShadowMirrorX,
    OutlineEnabled,
    OutlineThickness,
    OutlineColor,
    Count
};
constexpr std::size_t MaterialParameterCount = static_cast<std::size_t>(MaterialParameter::Count);
using MaterialOverrideMask = std::bitset<MaterialParameterCount>;
enum class TextureOverrideMode { Inherit, Replace, Disabled };
