// SPDX-License-Identifier: MIT
#pragma once
#include <bitset>
#include "Material.h"
#include "RenderResourceHandle.h"

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

struct MaterialTextureOverride
{
    TextureOverrideMode mode = TextureOverrideMode::Inherit;
    std::shared_ptr<Texture2D> texture;
    std::string source;
};

// 单层实例只保存父句柄和覆盖项，不拥有 Shader，也不复制 GPU 纹理。
struct MaterialInstance
{
    MaterialHandle parent;
    MaterialProperties values;
    MaterialOverrideMask overrides;
    std::array<MaterialTextureOverride, MaterialTextureSlotCount> textures;

    Material Resolve(const Material& base, const std::string& name) const;
};
MaterialProperties SanitizeMaterialProperties(const MaterialProperties& properties);
