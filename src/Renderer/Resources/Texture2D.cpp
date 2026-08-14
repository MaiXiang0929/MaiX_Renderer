// SPDX-License-Identifier: MIT
/// @file Texture2D.cpp
/// @brief 2D 纹理类的实现文件
/// @details 该文件实现了 Texture2D 类的核心功能，包括从 PNG 文件加载纹理、绑定纹理到指定的纹理单元等。
/// @author MaiX
/// @date 2026-08-11


#include "Texture2D.h"

#include <iostream>
#include <vector>

#include "lodepng.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"

Texture2D::~Texture2D()
{
    if (m_TextureID != 0)
    {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
}

std::shared_ptr<Texture2D> Texture2D::Load(
    const std::string& filePath,
    TextureColorSpace colorSpace)
{
    std::vector<unsigned char> image;
    unsigned int width = 0;
    unsigned int height = 0;
    const unsigned int error = lodepng::decode(image, width, height, filePath);

    if (error != 0)
    {
        std::cerr << "[Texture2D Error] " << lodepng_error_text(error)
                  << " File: " << filePath << std::endl;
        return nullptr;
    }

    return CreateRGBA8(width, height, image, colorSpace);
}

std::shared_ptr<Texture2D> Texture2D::CreateRGBA8(
    unsigned int width,
    unsigned int height,
    const std::vector<unsigned char>& pixels,
    TextureColorSpace colorSpace)
{
    const std::size_t expectedSize =
        static_cast<std::size_t>(width) * height * 4;
    if (width == 0 || height == 0 || pixels.size() != expectedSize)
    {
        std::cerr
            << "[Texture2D Error] Invalid RGBA8 texture extent or data size."
            << std::endl;
        return nullptr;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    const GLint internalFormat = colorSpace == TextureColorSpace::SRGB
        ? GL_SRGB8_ALPHA8
        : GL_RGBA8;
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return std::shared_ptr<Texture2D>(new Texture2D(textureID, colorSpace));
}

void Texture2D::Bind(unsigned int textureUnit) const
{
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_2D, textureUnit, m_TextureID);
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
}
