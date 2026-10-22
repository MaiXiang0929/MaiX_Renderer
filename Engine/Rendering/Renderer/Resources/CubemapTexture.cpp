// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/CubemapTexture.h"
#include "lodepng.h"
#include <vector>
#include <iostream>
CubemapTexture::~CubemapTexture()
{
    m_Device.DestroyTexture(m_TextureID);
}
bool CubemapTexture::Load(const std::string &directory)
{
    const char *names[] = {"posx", "negx", "posy", "negy", "posz", "negz"};
    std::array<std::vector<unsigned char>, 6> images;
    std::array<const void *, 6> data{};
    RHITextureDesc desc;
    desc.kind = RHITextureKind::Cubemap;
    desc.format = RHITextureFormat::SRGBA8;
    desc.wrap = RHIWrap::ClampEdge;
    desc.mipmaps = true; // Source mip chain used by solid-angle IBL sampling.
    for (unsigned face = 0; face < 6; ++face)
    {
        unsigned width = 0, height = 0;
        const auto path = directory + "/cubemap_" + names[face] + ".png";
        const auto error = lodepng::decode(images[face], width, height, path);
        if (error)
        {
            std::cerr << "[Cubemap] " << path << ": " << lodepng_error_text(error) << '\n';
            return false;
        }
        if (width != height || (face && (width != desc.width || height != desc.height)))
        {
            std::cerr << "[Cubemap] Inconsistent square face dimensions: " << path << '\n';
            return false;
        }
        desc.width = width;
        desc.height = height;
        data[face] = images[face].data();
    }
    const auto candidate = m_Device.CreateTexture(desc, data);
    if (candidate == 0)
        return false;
    struct ReleaseCandidate
    {
        RHIDevice& device; RHITextureHandle texture;
        ~ReleaseCandidate() { device.DestroyTexture(texture); }
    } release{m_Device,candidate};
    if (!m_Lighting.Build(candidate,desc.width))
    {
        std::cerr << "[Cubemap] IBL generation failed: " << directory << '\n';
        return false;
    }
    m_Device.DestroyTexture(m_TextureID);
    m_TextureID = candidate;
    m_Size = desc.width;
    release.texture = 0;
    return true;
}
void CubemapTexture::Bind(unsigned unit) const
{
    m_Device.Commands().BindTexture(m_TextureID, unit, RHITextureKind::Cubemap);
}
