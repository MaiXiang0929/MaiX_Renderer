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

#include <glad/glad.h>

#include "Renderer/Resources/TextureTypes.h"

class Texture2D
{
public:
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&&) = delete;
    Texture2D& operator=(Texture2D&&) = delete;

    static std::shared_ptr<Texture2D> Load(
        const std::string& filePath,
        TextureColorSpace colorSpace = TextureColorSpace::Linear);
    static std::shared_ptr<Texture2D> CreateRGBA8(
        unsigned int width,
        unsigned int height,
        const std::vector<unsigned char>& pixels,
        TextureColorSpace colorSpace = TextureColorSpace::Linear);

    void Bind(unsigned int textureUnit) const;
    bool IsValid() const { return m_TextureID != 0; }
    GLuint GetID() const { return m_TextureID; }
    TextureColorSpace GetColorSpace() const { return m_ColorSpace; }

private:
    Texture2D(GLuint textureID, TextureColorSpace colorSpace)
        : m_TextureID(textureID)
        , m_ColorSpace(colorSpace)
    {
    }

    GLuint m_TextureID = 0;
    TextureColorSpace m_ColorSpace = TextureColorSpace::Linear;
};
