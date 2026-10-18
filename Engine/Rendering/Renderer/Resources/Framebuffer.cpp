// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
Framebuffer::~Framebuffer()
{
    Cleanup();
}
bool Framebuffer::Init(int width, int height, FramebufferColorFormat format)
{
    FramebufferSpecification spec;
    spec.width = width;
    spec.height = height;
    spec.colorFormat = format;
    return Init(spec);
}
bool Framebuffer::Init(const FramebufferSpecification &spec)
{
    if (spec.width <= 0 || spec.height <= 0)
        return false;
    if (m_FBO != 0 && m_Width == spec.width && m_Height == spec.height && m_ColorFormat == spec.colorFormat &&
        m_DepthStencilEnabled == spec.depthStencilEnabled && m_SampleableDepth == spec.sampleableDepth &&
        m_MipmapsEnabled == spec.mipmapsEnabled)
        return true;
    RHITargetDesc desc;
    desc.width = spec.width;
    desc.height = spec.height;
    desc.colorFormat = spec.colorFormat == FramebufferColorFormat::RGBA16F ? RHITextureFormat::RGBA16F
                       : spec.colorFormat == FramebufferColorFormat::R8    ? RHITextureFormat::R8
                                                                           : RHITextureFormat::RGBA8;
    desc.depth = spec.depthStencilEnabled;
    desc.sampleableDepth = spec.sampleableDepth;
    desc.mipmaps = spec.mipmapsEnabled;
    const auto candidate = m_Device.CreateTarget(desc);
    if (candidate.handle == 0)
        return false;
    m_Device.RemapTargetBindings(m_FBO, candidate.handle);
    m_Device.DestroyTarget(m_FBO);
    m_FBO = candidate.handle;
    m_ColorTexture = candidate.color;
    m_DepthTexture = candidate.depth;
    m_Width = spec.width;
    m_Height = spec.height;
    m_ColorFormat = spec.colorFormat;
    m_DepthStencilEnabled = spec.depthStencilEnabled;
    m_SampleableDepth = spec.sampleableDepth;
    m_MipmapsEnabled = spec.mipmapsEnabled;
    return true;
}
void Framebuffer::Cleanup()
{
    m_Device.DestroyTarget(m_FBO);
    m_FBO = 0;
    m_ColorTexture = 0;
    m_DepthTexture = 0;
    m_Width = 0;
    m_Height = 0;
    m_ColorFormat = FramebufferColorFormat::RGBA8;
    m_DepthStencilEnabled = true;
    m_SampleableDepth = false;
    m_MipmapsEnabled = true;
}
void Framebuffer::Bind() const
{
    m_Device.Commands().BeginTarget(m_FBO, false, false, 1);
}
void Framebuffer::Unbind() const
{
    m_Device.Commands().EndTarget();
}
void Framebuffer::GenerateMipmaps() const
{
    if (m_MipmapsEnabled && m_ColorTexture != 0)
        m_Device.Commands().GenerateMipmaps(m_ColorTexture);
}
