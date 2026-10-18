// SPDX-License-Identifier: MIT
/// @file CubemapTexture.h
/// @brief CubemapTexture 类的声明文件
/// @details 该文件声明了 CubemapTexture 类，用于加载和管理立方体贴图纹理。
/// @author MaiX
/// @date 2026-08-11

#pragma once

#include <string>

#include "Engine/Rendering/RHI/RHIDevice.h"

class CubemapTexture
{
public:
    ~CubemapTexture();

    explicit CubemapTexture(RHIDevice& device) : m_Device(device) {}
    CubemapTexture(const CubemapTexture&) = delete;
    CubemapTexture& operator=(const CubemapTexture&) = delete;

    bool Load(const std::string& directoryPath);
    void Bind(unsigned int textureUnit) const;

    bool IsValid() const { return m_TextureID != 0; }
    RHITextureHandle GetID() const { return m_TextureID; }

private:
    RHIDevice& m_Device;
    RHITextureHandle m_TextureID = 0;
};
