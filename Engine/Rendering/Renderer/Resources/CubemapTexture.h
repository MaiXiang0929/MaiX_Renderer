// SPDX-License-Identifier: MIT
/// @file CubemapTexture.h
/// @brief CubemapTexture 类的声明文件
/// @details 该文件声明了 CubemapTexture 类，用于加载和管理立方体贴图纹理。
/// @author MaiX
/// @date 2026-08-11

#pragma once

#include <string>
#include <filesystem>

#include "Engine/Rendering/RHI/RHIDevice.h"
#include "Engine/Rendering/Renderer/Resources/EnvironmentLighting.h"

class CubemapTexture
{
public:
    ~CubemapTexture();

    explicit CubemapTexture(RHIDevice& device) : m_Device(device), m_Lighting(device) {}
    CubemapTexture(const CubemapTexture&) = delete;
    CubemapTexture& operator=(const CubemapTexture&) = delete;

    bool Load(const std::string& directoryPath);
    bool LoadHdr(const std::filesystem::path& path, std::string& error);
    bool IsHdr() const { return m_IsHdr; }
    unsigned Size() const { return m_Size; }
    bool RebuildLighting() { return m_Lighting.Build(m_TextureID,m_Size); }
    void Bind(unsigned int textureUnit) const;

    bool IsValid() const { return m_TextureID != 0; }
    RHITextureHandle GetID() const { return m_TextureID; }
    const EnvironmentLighting& Lighting() const { return m_Lighting; }

private:
    RHIDevice& m_Device;
    RHITextureHandle m_TextureID = 0;
    unsigned m_Size = 0;
    bool m_IsHdr = false;
    EnvironmentLighting m_Lighting;
};
