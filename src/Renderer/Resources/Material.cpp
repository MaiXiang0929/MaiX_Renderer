// SPDX-License-Identifier: MIT
/// @file Material.cpp
/// @brief Material 类的实现文件
/// @details 该文件实现了 Material 类的成员函数，用于绑定材质属性和纹理到着色器。
/// @author MaiX
/// @date 2026-08-11


#include "Material.h"

#include <iostream>
#include <utility>

#include "Shader.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"

namespace
{
const std::shared_ptr<Texture2D>& GetEmptyTexture()
{
    static const std::shared_ptr<Texture2D> empty;
    return empty;
}

const char* GetSlotName(MaterialTextureSlot slot)
{
    switch (slot)
    {
    case MaterialTextureSlot::BaseColor: return "BaseColor";
    case MaterialTextureSlot::Normal: return "Normal";
    case MaterialTextureSlot::OcclusionRoughnessMetallic: return "ORM";
    case MaterialTextureSlot::Displacement: return "Displacement";
    case MaterialTextureSlot::LegacySpecular: return "LegacySpecular";
    case MaterialTextureSlot::Count: break;
    }
    return "Invalid";
}
}

void Material::Bind(Shader& shader, unsigned int firstTextureUnit) const
{
    RenderSubmissionStats::Get().RecordMaterialBind(this);
    shader.SetVec3(
        "material.baseColor",
        m_Properties.baseColor.x,
        m_Properties.baseColor.y,
        m_Properties.baseColor.z);
    shader.SetVec3(
        "material.specularColor",
        m_Properties.specularColor.x,
        m_Properties.specularColor.y,
        m_Properties.specularColor.z);
    shader.SetFloat("material.shininess", m_Properties.shininess);
    shader.SetFloat(
        "material.environmentReflectivity",
        m_Properties.environmentReflectivity);
    shader.SetFloat("material.metallic", m_Properties.metallic);
    shader.SetFloat("material.roughness", m_Properties.roughness);
    shader.SetFloat("material.ambientOcclusion", m_Properties.ambientOcclusion);
    shader.SetFloat("material.normalScale", m_Properties.normalScale);
    shader.SetFloat("material.opacity", m_Properties.opacity);

    const unsigned int albedoUnit = firstTextureUnit +
        GetMaterialTextureUnitOffset(MaterialTextureSlot::BaseColor);
    const unsigned int packedDataUnit = firstTextureUnit +
        GetMaterialTextureUnitOffset(
            MaterialTextureSlot::OcclusionRoughnessMetallic);
    const unsigned int normalUnit = firstTextureUnit +
        GetMaterialTextureUnitOffset(MaterialTextureSlot::Normal);
    const unsigned int displacementUnit = firstTextureUnit +
        GetMaterialTextureUnitOffset(MaterialTextureSlot::Displacement);

    const auto& albedoMap =
        m_Textures[ToIndex(MaterialTextureSlot::BaseColor)];
    const auto& normalMap =
        m_Textures[ToIndex(MaterialTextureSlot::Normal)];
    const auto& ormMap = m_Textures[
        ToIndex(MaterialTextureSlot::OcclusionRoughnessMetallic)];
    const auto& displacementMap =
        m_Textures[ToIndex(MaterialTextureSlot::Displacement)];
    const auto& specularMap =
        m_Textures[ToIndex(MaterialTextureSlot::LegacySpecular)];

    shader.SetInt("material.hasAlbedoMap", albedoMap ? 1 : 0);
    if (albedoMap)
    {
        albedoMap->Bind(albedoUnit);
        shader.SetInt("material.albedoMap", static_cast<int>(albedoUnit));
    }

    shader.SetInt("material.hasOrmMap", ormMap ? 1 : 0);
    shader.SetInt(
        "material.hasSpecularMap", !ormMap && specularMap ? 1 : 0);
    if (ormMap)
    {
        ormMap->Bind(packedDataUnit);
        shader.SetInt("material.ormMap", static_cast<int>(packedDataUnit));
    }
    else if (specularMap)
    {
        specularMap->Bind(packedDataUnit);
        shader.SetInt(
            "material.specularMap", static_cast<int>(packedDataUnit));
    }

    shader.SetInt("material.hasNormalMap", normalMap ? 1 : 0);
    if (normalMap)
    {
        normalMap->Bind(normalUnit);
        shader.SetInt("material.normalMap", static_cast<int>(normalUnit));
    }

    BindDisplacement(shader, displacementUnit);
}

bool Material::SetTexture(
    MaterialTextureSlot slot,
    std::shared_ptr<Texture2D> texture)
{
    if (ToIndex(slot) >= MaterialTextureSlotCount)
        return false;
    if (texture && texture->GetColorSpace() !=
        GetRequiredMaterialTextureColorSpace(slot))
    {
        std::cerr
            << "[Material] Rejected " << GetSlotName(slot)
            << " texture with incompatible color space."
            << std::endl;
        return false;
    }

    m_Textures[ToIndex(slot)] = std::move(texture);
    return true;
}

const std::shared_ptr<Texture2D>& Material::GetTexture(
    MaterialTextureSlot slot) const
{
    if (ToIndex(slot) >= MaterialTextureSlotCount)
        return GetEmptyTexture();
    return m_Textures[ToIndex(slot)];
}

void Material::SetAlbedoMap(std::shared_ptr<Texture2D> texture)
{
    SetTexture(MaterialTextureSlot::BaseColor, std::move(texture));
}

void Material::SetSpecularMap(std::shared_ptr<Texture2D> texture)
{
    SetTexture(MaterialTextureSlot::LegacySpecular, std::move(texture));
}

void Material::SetNormalMap(std::shared_ptr<Texture2D> texture)
{
    SetTexture(MaterialTextureSlot::Normal, std::move(texture));
}

void Material::SetOcclusionRoughnessMetallicMap(
    std::shared_ptr<Texture2D> texture)
{
    SetTexture(
        MaterialTextureSlot::OcclusionRoughnessMetallic,
        std::move(texture));
}

void Material::SetDisplacementMap(std::shared_ptr<Texture2D> texture)
{
    SetTexture(MaterialTextureSlot::Displacement, std::move(texture));
}

void Material::BindDisplacement(Shader& shader, unsigned int textureUnit) const
{
    const auto& displacementMap =
        m_Textures[ToIndex(MaterialTextureSlot::Displacement)];
    const int hasDisplacement = displacementMap ? 1 : 0;
    shader.SetInt("material.hasDisplacementMap", hasDisplacement);
    shader.SetInt("hasDisplacementMap", hasDisplacement);
    if (!displacementMap)
        return;

    displacementMap->Bind(textureUnit);
    shader.SetInt("material.displacementMap", static_cast<int>(textureUnit));
    shader.SetInt("displacementMap", static_cast<int>(textureUnit));
}
