// SPDX-License-Identifier: MIT
/// @file Framebuffer.cpp
/// @brief 帧缓冲区管理类的实现文件
/// @details 封装 OpenGL Framebuffer Object (FBO)，提供离屏渲染与纹理附着能力。
/// @author MaiX
/// @date 2026-08-04

#include "Framebuffer.h"
#include <cstring>
#include <iostream>
#include <utility>

// 匿名命名空间中的名称只在当前 Framebuffer.cpp 文件内可见，其他 .cpp 文件无法访问
namespace {
    // GLAD 仅生成核心 API，因此在本文件中补充各向异性过滤扩展枚举值。
    constexpr GLenum TextureMaxAnisotropyExt = 0x84FE;          // 设置某张纹理的各向异性等级
    constexpr GLenum MaxTextureMaxAnisotropyExt = 0x84FF;       // 查询显卡支持的最大等级

    // 各向异性过滤不是 OpenGL 4.0 核心功能，设置参数前必须检查驱动扩展列表。
    bool SupportsAnisotropicFiltering() {
        GLint extensionCount = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
        for (GLint i = 0; i < extensionCount; ++i) {
            const char* extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (extension != nullptr &&
                std::strcmp(extension, "GL_EXT_texture_filter_anisotropic") == 0) {
                return true;
            }
        }
        return false;
    }

    // Init 会临时绑定候选资源；销毁旧资源前把旧绑定映射到新资源。
    class ScopedFramebufferBindings {
    public:
        ScopedFramebufferBindings() {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_DrawFramebuffer);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_ReadFramebuffer);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_Texture);
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &m_Renderbuffer);
        }

        ~ScopedFramebufferBindings() {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                static_cast<GLuint>(m_DrawFramebuffer));
            glBindFramebuffer(GL_READ_FRAMEBUFFER,
                static_cast<GLuint>(m_ReadFramebuffer));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_Texture));
            glBindRenderbuffer(GL_RENDERBUFFER,
                static_cast<GLuint>(m_Renderbuffer));
        }

        void Remap(GLuint oldFramebuffer, GLuint newFramebuffer,
                   GLuint oldColorTexture, GLuint newColorTexture,
                   GLuint oldDepthTexture, GLuint newDepthTexture,
                   GLuint oldRenderbuffer, GLuint newRenderbuffer) {
            if (m_DrawFramebuffer == static_cast<GLint>(oldFramebuffer) &&
                oldFramebuffer != 0)
                m_DrawFramebuffer = static_cast<GLint>(newFramebuffer);
            if (m_ReadFramebuffer == static_cast<GLint>(oldFramebuffer) &&
                oldFramebuffer != 0)
                m_ReadFramebuffer = static_cast<GLint>(newFramebuffer);
            if (m_Texture == static_cast<GLint>(oldColorTexture) &&
                oldColorTexture != 0)
                m_Texture = static_cast<GLint>(newColorTexture);
            if (m_Texture == static_cast<GLint>(oldDepthTexture) &&
                oldDepthTexture != 0)
                m_Texture = static_cast<GLint>(newDepthTexture);
            if (m_Renderbuffer == static_cast<GLint>(oldRenderbuffer) &&
                oldRenderbuffer != 0)
                m_Renderbuffer = static_cast<GLint>(newRenderbuffer);
        }

    private:
        GLint m_DrawFramebuffer = 0;
        GLint m_ReadFramebuffer = 0;
        GLint m_Texture = 0;
        GLint m_Renderbuffer = 0;
    };
}

Framebuffer::~Framebuffer() {
    Cleanup();
}

bool Framebuffer::Init(
    int width,
    int height,
    FramebufferColorFormat colorFormat) {
    FramebufferSpecification specification;
    specification.width = width;
    specification.height = height;
    specification.colorFormat = colorFormat;
    return Init(specification);
}

bool Framebuffer::Init(const FramebufferSpecification& specification) {
    if (specification.width <= 0 || specification.height <= 0) {
        std::cerr << "[Framebuffer] Rejected invalid extent "
                  << specification.width << "x" << specification.height
                  << "." << std::endl;
        return false;
    }
    if (m_Width == specification.width &&
        m_Height == specification.height &&
        m_ColorFormat == specification.colorFormat &&
        m_DepthStencilEnabled == specification.depthStencilEnabled &&
        m_SampleableDepth == specification.sampleableDepth &&
        m_MipmapsEnabled == specification.mipmapsEnabled &&
        m_FBO != 0) return true;

    ScopedFramebufferBindings bindings;
    Framebuffer candidate;
    candidate.m_Width = specification.width;
    candidate.m_Height = specification.height;
    candidate.m_ColorFormat = specification.colorFormat;
    candidate.m_DepthStencilEnabled = specification.depthStencilEnabled;
    candidate.m_SampleableDepth = specification.sampleableDepth;
    candidate.m_MipmapsEnabled = specification.mipmapsEnabled;

    // 1. 创建 FBO
    glGenFramebuffers(1, &candidate.m_FBO);
    if (candidate.m_FBO == 0) {
        std::cerr << "[Framebuffer] Unable to create framebuffer."
                  << std::endl;
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, candidate.m_FBO);

    // 2. 创建 Color Texture Attachment
    glGenTextures(1, &candidate.m_ColorTexture);
    if (candidate.m_ColorTexture == 0) {
        std::cerr << "[Framebuffer] Unable to create color texture."
                  << std::endl;
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, candidate.m_ColorTexture);
    const GLint internalFormat =
        candidate.m_ColorFormat == FramebufferColorFormat::RGBA16F ? GL_RGBA16F
        : candidate.m_ColorFormat == FramebufferColorFormat::R8 ? GL_R8 : GL_RGBA8;
    const GLenum pixelFormat =
        candidate.m_ColorFormat == FramebufferColorFormat::R8 ? GL_RED : GL_RGBA;
    const GLenum dataType = candidate.m_ColorFormat == FramebufferColorFormat::RGBA16F
        ? GL_FLOAT
        : GL_UNSIGNED_BYTE;
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        candidate.m_Width,
        candidate.m_Height,
        0,
        pixelFormat,
        dataType,
        nullptr);

    if (candidate.m_MipmapsEnabled)
        glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        candidate.m_MipmapsEnabled ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 驱动支持时使用硬件允许的最大各向异性等级，改善倾斜平面的缩小采样质量。
    if (candidate.m_MipmapsEnabled && SupportsAnisotropicFiltering()) {
        GLfloat maxAnisotropy = 1.0f;
        glGetFloatv(MaxTextureMaxAnisotropyExt, &maxAnisotropy);
        glTexParameterf(GL_TEXTURE_2D, TextureMaxAnisotropyExt, maxAnisotropy);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, candidate.m_ColorTexture, 0);

    if (candidate.m_DepthStencilEnabled && candidate.m_SampleableDepth) {
        glGenTextures(1, &candidate.m_DepthTexture);
        if (candidate.m_DepthTexture == 0) {
            std::cerr << "[Framebuffer] Unable to create depth texture."
                      << std::endl;
            return false;
        }
        glBindTexture(GL_TEXTURE_2D, candidate.m_DepthTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH24_STENCIL8,
            candidate.m_Width,
            candidate.m_Height,
            0,
            GL_DEPTH_STENCIL,
            GL_UNSIGNED_INT_24_8,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D,
            candidate.m_DepthTexture,
            0);
    }
    else if (candidate.m_DepthStencilEnabled) {
        glGenRenderbuffers(1, &candidate.m_RBO);
        if (candidate.m_RBO == 0) {
            std::cerr << "[Framebuffer] Unable to create depth renderbuffer."
                      << std::endl;
            return false;
        }
        glBindRenderbuffer(GL_RENDERBUFFER, candidate.m_RBO);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            candidate.m_Width, candidate.m_Height);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            candidate.m_RBO);
    }

    // 4. 检查 FBO 完整性
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Framebuffer] Incomplete framebuffer at "
                  << candidate.m_Width << "x" << candidate.m_Height
                  << " (status=0x" << std::hex << status << std::dec << ")."
                  << std::endl;
        return false;
    }

    bindings.Remap(m_FBO, candidate.m_FBO,
        m_ColorTexture, candidate.m_ColorTexture,
        m_DepthTexture, candidate.m_DepthTexture,
        m_RBO, candidate.m_RBO);
    Swap(candidate);
    return true;
}

void Framebuffer::Swap(Framebuffer& other) noexcept {
    using std::swap;
    swap(m_FBO, other.m_FBO);
    swap(m_ColorTexture, other.m_ColorTexture);
    swap(m_DepthTexture, other.m_DepthTexture);
    swap(m_RBO, other.m_RBO);
    swap(m_Width, other.m_Width);
    swap(m_Height, other.m_Height);
    swap(m_ColorFormat, other.m_ColorFormat);
    swap(m_DepthStencilEnabled, other.m_DepthStencilEnabled);
    swap(m_SampleableDepth, other.m_SampleableDepth);
    swap(m_MipmapsEnabled, other.m_MipmapsEnabled);
}

void Framebuffer::Cleanup() {
    if (m_RBO != 0) { glDeleteRenderbuffers(1, &m_RBO); m_RBO = 0; }
    if (m_DepthTexture != 0) { glDeleteTextures(1, &m_DepthTexture); m_DepthTexture = 0; }
    if (m_ColorTexture != 0) { glDeleteTextures(1, &m_ColorTexture); m_ColorTexture = 0; }
    if (m_FBO != 0) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    m_Width = 0;
    m_Height = 0;
    m_ColorFormat = FramebufferColorFormat::RGBA8;
    m_DepthStencilEnabled = true;
    m_SampleableDepth = false;
    m_MipmapsEnabled = true;
}

void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
}

void Framebuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::GenerateMipmaps() const {
    if (!m_MipmapsEnabled || m_ColorTexture == 0)
        return;
    // 颜色附件每帧都会被重绘，旧的低分辨率层级必须随之更新。
    glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}
