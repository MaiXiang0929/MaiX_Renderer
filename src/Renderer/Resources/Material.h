// SPDX-License-Identifier: MIT
/// @file Material.h
/// @brief Material 类的声明文件
/// @details 该文件声明了 Material 类及其相关结构体，用于管理材质属性和纹理。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <array>
#include <memory>
#include <string>

#include "cyVector.h"

#include "Renderer/Resources/Texture2D.h"
#include "Renderer/Resources/MaterialTypes.h"

class Shader;

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
    bool outlineEnabled = false;
    float outlineThickness = 0.025f;
    cy::Vec3f outlineColor{ 0.02f, 0.02f, 0.02f };
};

class Material
{
public:
    void Bind(Shader& shader, unsigned int firstTextureUnit = 0) const;

    void SetName(std::string name);
    const std::string& GetName() const { return m_Name; }

    void SetBlendMode(BlendMode blendMode) { m_BlendMode = blendMode; }
    BlendMode GetBlendMode() const { return m_BlendMode; }

    MaterialProperties& GetProperties() { return m_Properties; }
    const MaterialProperties& GetProperties() const { return m_Properties; }

    bool SetTexture(
        MaterialTextureSlot slot,
        std::shared_ptr<Texture2D> texture,
        std::string sourceLabel = {});
    const std::shared_ptr<Texture2D>& GetTexture(
        MaterialTextureSlot slot) const;
    const std::string& GetTextureSource(MaterialTextureSlot slot) const;
    bool ClearTexture(MaterialTextureSlot slot);

    void SetAlbedoMap(std::shared_ptr<Texture2D> texture);
    void SetSpecularMap(std::shared_ptr<Texture2D> texture);
    void SetNormalMap(std::shared_ptr<Texture2D> texture);
    void SetOcclusionRoughnessMetallicMap(
        std::shared_ptr<Texture2D> texture);
    void SetDisplacementMap(std::shared_ptr<Texture2D> texture);
    void BindDisplacement(Shader& shader, unsigned int textureUnit) const;

private:
    std::string m_Name;
    BlendMode m_BlendMode = BlendMode::Opaque;
    MaterialProperties m_Properties;
    std::array<std::shared_ptr<Texture2D>, MaterialTextureSlotCount> m_Textures;
    std::array<std::string, MaterialTextureSlotCount> m_TextureSources;
};
