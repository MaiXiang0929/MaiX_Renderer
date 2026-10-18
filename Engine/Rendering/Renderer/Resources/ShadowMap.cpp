// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/ShadowMap.h"
ShadowMap::~ShadowMap()
{
    Cleanup();
}
bool ShadowMap::Init(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;
    if (m_FBO != 0 && width == m_Width && height == m_Height)
        return true;
    RHITargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.color = false;
    desc.shadow = true;
    const auto candidate = m_Device.CreateTarget(desc);
    if (candidate.handle == 0)
        return false;
    m_Device.RemapTargetBindings(m_FBO, candidate.handle);
    Cleanup();
    m_FBO = candidate.handle;
    m_DepthTexture = candidate.depth;
    m_Width = width;
    m_Height = height;
    return true;
}
void ShadowMap::Cleanup()
{
    m_Device.DestroyTarget(m_FBO);
    m_FBO = 0;
    m_DepthTexture = 0;
    m_Width = 0;
    m_Height = 0;
}
void ShadowMap::BindForWriting() const
{
    m_Device.Commands().BeginTarget(m_FBO, false, false, 1);
}
void ShadowMap::Unbind() const
{
    m_Device.Commands().EndTarget();
}
