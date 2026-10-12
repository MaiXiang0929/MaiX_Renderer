// SPDX-License-Identifier: MIT
/// @file CubemapTexture.cpp
/// @brief CubemapTexture 类的实现文件
/// @details 该文件实现了 CubemapTexture 类的核心功能，包括加载立方体贴图纹理和绑定纹理单元。
/// @author MaiX
/// @date 2026-08-11

#include "Engine/Rendering/Renderer/Resources/CubemapTexture.h"

#include <iostream>
#include <vector>

#include "lodepng.h"
#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"

CubemapTexture::~CubemapTexture()
{
    if (m_TextureID != 0)
        glDeleteTextures(1, &m_TextureID);
}

bool CubemapTexture::Load(const std::string& directoryPath)
{
    static const char* faceNames[6] = {
        "posx", "negx", "posy", "negy", "posz", "negz"
    };

    GLuint newTexture = 0;
    glGenTextures(1, &newTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, newTexture);

    for (int face = 0; face < 6; ++face)
    {
        const std::string filePath = directoryPath +
            "/cubemap_" + faceNames[face] + ".png";
        std::vector<unsigned char> image;
        unsigned int width = 0;
        unsigned int height = 0;
        const unsigned int error = lodepng::decode(image, width, height, filePath);
        if (error != 0)
        {
            std::cerr << "[Cubemap Error] " << lodepng_error_text(error)
                      << " File: " << filePath << std::endl;
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glDeleteTextures(1, &newTexture);
            return false;
        }

        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            0,
            GL_SRGB8_ALPHA8,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            image.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    if (m_TextureID != 0)
        glDeleteTextures(1, &m_TextureID);
    m_TextureID = newTexture;
    return true;
}

void CubemapTexture::Bind(unsigned int textureUnit) const
{
    RenderSubmissionStats::Get().RecordTextureBind(
        GL_TEXTURE_CUBE_MAP, textureUnit, m_TextureID);
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_TextureID);
}
