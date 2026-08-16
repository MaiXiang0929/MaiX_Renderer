// SPDX-License-Identifier: MIT
/// @file Framebuffer.h
/// @brief 帧缓冲区管理类的头文件
/// @details 封装 OpenGL Framebuffer Object (FBO)，提供离屏渲染与纹理附着能力。
/// @author MaiX
/// @date 2026-08-04

#pragma once

#include <glad/glad.h>

enum class FramebufferColorFormat
{
    RGBA8,
    RGBA16F,
    R8
};

struct FramebufferSpecification
{
    int width = 0;
    int height = 0;
    FramebufferColorFormat colorFormat = FramebufferColorFormat::RGBA8;
    bool depthStencilEnabled = true;
    bool sampleableDepth = false;
    bool mipmapsEnabled = true;
};

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    // 禁止复制
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    /// @brief 初始化或重新创建 Framebuffer
    /// @param width 缓冲区宽度
    /// @param height 缓冲区高度
    bool Init(
        int width,
        int height,
        FramebufferColorFormat colorFormat = FramebufferColorFormat::RGBA8);
    bool Init(const FramebufferSpecification& specification);

    /// @brief 释放内部 OpenGL 资源
    void Cleanup();

    /// @brief 绑定当前 FBO 以进行离屏渲染
    void Bind() const;

    /// @brief 解绑 FBO，恢复到默认屏幕缓冲区 (ID 0)
    void Unbind() const;

    /// @brief 离屏渲染完成后，依据颜色附件的最新内容重新生成 MipMap
    void GenerateMipmaps() const;

    /// @brief 获取生成的离屏颜色纹理 ID
    GLuint GetColorTexture() const { return m_ColorTexture; }

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    FramebufferColorFormat GetColorFormat() const { return m_ColorFormat; }
    bool HasDepthStencil() const { return m_DepthStencilEnabled; }
    bool HasDepthTexture() const { return m_DepthTexture != 0; }
    GLuint GetDepthTexture() const { return m_DepthTexture; }
    bool HasMipmaps() const { return m_MipmapsEnabled; }

private:
    GLuint m_FBO = 0;
    GLuint m_ColorTexture = 0;
    GLuint m_DepthTexture = 0;
    GLuint m_RBO = 0; // 深度/模板 Renderbuffer

    int m_Width = 0;
    int m_Height = 0;
    FramebufferColorFormat m_ColorFormat = FramebufferColorFormat::RGBA8;
    bool m_DepthStencilEnabled = true;
    bool m_SampleableDepth = false;
    bool m_MipmapsEnabled = true;
};
