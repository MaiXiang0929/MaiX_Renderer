// SPDX-License-Identifier: MIT
/// @file Texture2D.h
/// @brief 2D 纹理类的头文件
/// @details 该文件声明了 Texture2D 类，用于加载和管理 2D 纹理资源，包括从 PNG 文件加载纹理、绑定纹理到指定的纹理单元等功能。
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/Rendering/RHI/RHIDevice.h"

#include "Engine/Rendering/Renderer/Resources/TextureTypes.h"

class Texture2D
{
public:
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&&) = delete;
    Texture2D& operator=(Texture2D&&) = delete;

    static std::shared_ptr<Texture2D> Load(
        RHIDevice& device, const std::string& filePath,
        TextureColorSpace colorSpace = TextureColorSpace::Linear);
    static std::shared_ptr<Texture2D> CreateRGBA8(
        RHIDevice& device, unsigned int width,
        unsigned int height,
        const std::vector<unsigned char>& pixels,
        TextureColorSpace colorSpace = TextureColorSpace::Linear);

    void Bind(RHIDevice& consumer, unsigned int textureUnit) const;
    bool IsValid() const { return !m_DeviceLifetime.expired() && m_TextureID != 0; }
    RHITextureHandle GetID() const { return IsValid() ? m_TextureID : RHITextureHandle{}; }
    TextureColorSpace GetColorSpace() const { return m_ColorSpace; }

private:
    RHIDevice& m_Device;
    std::weak_ptr<void> m_DeviceLifetime;
    Texture2D(RHIDevice& device, RHITextureHandle textureID, TextureColorSpace colorSpace)
        : m_Device(device), m_DeviceLifetime(device.Lifetime()), m_TextureID(textureID)
        , m_ColorSpace(colorSpace)
    {
    }

    RHITextureHandle m_TextureID = 0;
    TextureColorSpace m_ColorSpace = TextureColorSpace::Linear;
};
