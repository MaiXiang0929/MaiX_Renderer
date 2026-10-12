// SPDX-License-Identifier: MIT
/// @file ShadowMap.cpp
/// @brief 阴影深度纹理及其帧缓冲资源的生命周期封装
/// @details 
/// @author MaiX
/// @date 2026-08-11


#include "Engine/Rendering/Renderer/Resources/ShadowMap.h"

#include <iostream>

ShadowMap::~ShadowMap() {
    Cleanup();
}

bool ShadowMap::Init(int width, int height) {
    if (width <= 0 || height <= 0) {
        std::cerr << "[ShadowMap] Invalid shadow map size." << std::endl;
        return false;
    }

    if (m_FBO != 0 && m_Width == width && m_Height == height) {
        return true;
    }

    Cleanup();
    m_Width = width;
    m_Height = height;

    glGenFramebuffers(1, &m_FBO);
    glGenTextures(1, &m_DepthTexture);

    glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT24,
        m_Width,
        m_Height,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);

    // 由 GPU 执行参考深度比较；线性过滤会对相邻比较结果做硬件 PCF。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const GLfloat borderDepth[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderDepth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        m_DepthTexture,
        0);

    // 深度 pass 不写颜色，显式关闭读写颜色附件才能保证深度 FBO 完整。
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete) {
        std::cerr << "[ShadowMap] Depth framebuffer is not complete." << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!complete) {
        Cleanup();
    }
    return complete;
}

void ShadowMap::Cleanup() {
    if (m_DepthTexture != 0) {
        glDeleteTextures(1, &m_DepthTexture);
        m_DepthTexture = 0;
    }
    if (m_FBO != 0) {
        glDeleteFramebuffers(1, &m_FBO);
        m_FBO = 0;
    }
    m_Width = 0;
    m_Height = 0;
}

void ShadowMap::BindForWriting() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
}

void ShadowMap::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
