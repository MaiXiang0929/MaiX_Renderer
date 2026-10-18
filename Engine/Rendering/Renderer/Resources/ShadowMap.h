// SPDX-License-Identifier: MIT
/// @file ShadowMap.h
/// @brief 阴影深度纹理及其帧缓冲资源的生命周期封装
/// @details 
/// @author MaiX
/// @date 2026-08-11


#pragma once

#include "Engine/Rendering/RHI/RHIDevice.h"

/// @brief 管理聚光灯阴影贴图所需的深度纹理和深度专用 FBO
class ShadowMap {
public:
    explicit ShadowMap(RHIDevice& device) : m_Device(device) {}
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    /// @brief 创建固定分辨率的深度比较纹理
    /// @return FBO 完整时返回 true
    bool Init(int width, int height);

    /// @brief 释放 OpenGL 资源；可安全重复调用
    void Cleanup();

    /// @brief 绑定深度 FBO，并把 viewport 切换到阴影贴图尺寸
    void BindForWriting() const;

    /// @brief 绑定回默认 FBO
    void Unbind() const;

    RHITextureHandle GetDepthTexture() const { return m_DepthTexture; }
    RHITargetHandle GetFramebufferId() const { return m_FBO; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

private:
    RHIDevice& m_Device;
    RHITargetHandle m_FBO = 0;
    RHITextureHandle m_DepthTexture = 0;
    int m_Width = 0;
    int m_Height = 0;
};
