// SPDX-License-Identifier: MIT
#include "MaterialInstance.h"
#include <algorithm>
#include <cmath>
namespace
{
constexpr float MinimumFaceAxisLength = 1.0e-5f;

void NormalizeFaceFrame(MaterialProperties& properties)
{
    cy::Vec3f forward = properties.faceForwardLocal;
    if (forward.Length() <= MinimumFaceAxisLength)
        forward = cy::Vec3f(0.0f, 0.0f, 1.0f);
    forward.Normalize();

    cy::Vec3f right = properties.faceRightLocal;
    right = right - forward * right.Dot(forward);
    if (right.Length() <= MinimumFaceAxisLength)
    {
        right = std::abs(forward.x) < 0.9f
            ? cy::Vec3f(1.0f, 0.0f, 0.0f)
            : cy::Vec3f(0.0f, 1.0f, 0.0f);
        right = right - forward * right.Dot(forward);
    }
    right.Normalize();

    properties.faceForwardLocal = forward;
    properties.faceRightLocal = right;
}
}


MaterialProperties SanitizeMaterialProperties(const MaterialProperties& properties)
{
    MaterialProperties clamped = properties;
    clamped.metallic = std::clamp(clamped.metallic, 0.0f, 1.0f);
    clamped.roughness = std::clamp(clamped.roughness, 0.045f, 1.0f);
    clamped.ambientOcclusion = std::clamp(clamped.ambientOcclusion, 0.0f, 1.0f);
    clamped.normalScale = std::clamp(clamped.normalScale, 0.0f, 4.0f);
    clamped.opacity = std::clamp(clamped.opacity, 0.0f, 1.0f);
    clamped.toonThreshold = std::clamp(clamped.toonThreshold, 0.0f, 1.0f);
    clamped.toonShadowStrength = std::clamp(clamped.toonShadowStrength, 0.0f, 1.0f);
    clamped.rimLightStrength = std::clamp(clamped.rimLightStrength, 0.0f, 4.0f);
    clamped.faceShadowSoftness = std::clamp(
        clamped.faceShadowSoftness,
        MinimumFaceShadowSoftness,
        MaximumFaceShadowSoftness);
    NormalizeFaceFrame(clamped);
    clamped.outlineThickness = ClampOutlineThickness(clamped.outlineThickness);
    clamped.baseColor.x = std::clamp(clamped.baseColor.x, 0.0f, 1.0f);
    clamped.baseColor.y = std::clamp(clamped.baseColor.y, 0.0f, 1.0f);
    clamped.baseColor.z = std::clamp(clamped.baseColor.z, 0.0f, 1.0f);
    clamped.toonShadowColor.x = std::clamp(clamped.toonShadowColor.x, 0.0f, 1.0f);
    clamped.toonShadowColor.y = std::clamp(clamped.toonShadowColor.y, 0.0f, 1.0f);
    clamped.toonShadowColor.z = std::clamp(clamped.toonShadowColor.z, 0.0f, 1.0f);
    clamped.rimLightColor.x = std::clamp(clamped.rimLightColor.x, 0.0f, 1.0f);
    clamped.rimLightColor.y = std::clamp(clamped.rimLightColor.y, 0.0f, 1.0f);
    clamped.rimLightColor.z = std::clamp(clamped.rimLightColor.z, 0.0f, 1.0f);
    clamped.outlineColor.x = std::clamp(clamped.outlineColor.x, 0.0f, 1.0f);
    clamped.outlineColor.y = std::clamp(clamped.outlineColor.y, 0.0f, 1.0f);
    clamped.outlineColor.z = std::clamp(clamped.outlineColor.z, 0.0f, 1.0f);

    return clamped;
}
Material MaterialInstance::Resolve(const Material& base, const std::string& name) const
{
    Material result = base;
    result.SetName(name);
    auto& resolved = result.GetProperties();
    const auto overridden = [&](MaterialParameter parameter) {
        return overrides.test(static_cast<std::size_t>(parameter));
    };
    if (overridden(MaterialParameter::BaseColor)) resolved.baseColor = values.baseColor;
    if (overridden(MaterialParameter::SpecularColor)) resolved.specularColor = values.specularColor;
    if (overridden(MaterialParameter::Shininess)) resolved.shininess = values.shininess;
    if (overridden(MaterialParameter::EnvironmentReflectivity)) resolved.environmentReflectivity = values.environmentReflectivity;
    if (overridden(MaterialParameter::Metallic)) resolved.metallic = values.metallic;
    if (overridden(MaterialParameter::Roughness)) resolved.roughness = values.roughness;
    if (overridden(MaterialParameter::AmbientOcclusion)) resolved.ambientOcclusion = values.ambientOcclusion;
    if (overridden(MaterialParameter::NormalScale)) resolved.normalScale = values.normalScale;
    if (overridden(MaterialParameter::Opacity)) resolved.opacity = values.opacity;
    if (overridden(MaterialParameter::ToonThreshold)) resolved.toonThreshold = values.toonThreshold;
    if (overridden(MaterialParameter::ToonShadowStrength)) resolved.toonShadowStrength = values.toonShadowStrength;
    if (overridden(MaterialParameter::ToonShadowColor)) resolved.toonShadowColor = values.toonShadowColor;
    if (overridden(MaterialParameter::RimLightStrength)) resolved.rimLightStrength = values.rimLightStrength;
    if (overridden(MaterialParameter::RimLightColor)) resolved.rimLightColor = values.rimLightColor;
    if (overridden(MaterialParameter::FaceShadowEnabled)) resolved.faceShadowEnabled = values.faceShadowEnabled;
    if (overridden(MaterialParameter::FaceFrame)) { resolved.faceForwardLocal = values.faceForwardLocal; resolved.faceRightLocal = values.faceRightLocal; }
    if (overridden(MaterialParameter::FaceShadowSoftness)) resolved.faceShadowSoftness = values.faceShadowSoftness;
    if (overridden(MaterialParameter::FaceShadowMirrorX)) resolved.faceShadowMirrorX = values.faceShadowMirrorX;
    if (overridden(MaterialParameter::OutlineEnabled)) resolved.outlineEnabled = values.outlineEnabled;
    if (overridden(MaterialParameter::OutlineThickness)) resolved.outlineThickness = values.outlineThickness;
    if (overridden(MaterialParameter::OutlineColor)) resolved.outlineColor = values.outlineColor;
    resolved = SanitizeMaterialProperties(resolved);
    // Shading Model 与 Blend Mode 始终由基础材质决定，实例不能覆盖队列策略。
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        const auto slot = static_cast<MaterialTextureSlot>(i);
        if (textures[i].mode == TextureOverrideMode::Replace)
            result.SetTexture(slot, textures[i].texture, textures[i].source);
        else if (textures[i].mode == TextureOverrideMode::Disabled)
            result.ClearTexture(slot);
    }
    return result;
}
