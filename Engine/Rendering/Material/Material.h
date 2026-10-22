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

#include "Engine/Rendering/Renderer/Resources/Texture2D.h"
#include "Engine/Rendering/Material/MaterialTypes.h"

class Shader;

#include "Engine/Rendering/Material/MaterialProperties.h"

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
