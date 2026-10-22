// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/Texture2D.h"
#include "lodepng.h"
#include <iostream>
#include <limits>
Texture2D::~Texture2D()
{
    if (!m_DeviceLifetime.expired())
        m_Device.DestroyTexture(m_TextureID);
}
std::shared_ptr<Texture2D> Texture2D::Load(RHIDevice &device, const std::string &path,
                                           TextureColorSpace space)
{
    std::vector<unsigned char> pixels;
    unsigned width = 0, height = 0;
    const auto error = lodepng::decode(pixels, width, height, path);
    if (error)
    {
        std::cerr << "[Texture2D] " << path << ": " << lodepng_error_text(error) << '\n';
        return {};
    }
    auto result = CreateRGBA8(device, width, height, pixels, space);
    if (result) result->SetSource(std::filesystem::absolute(std::filesystem::u8path(path)));
    return result;
}
std::shared_ptr<Texture2D> Texture2D::CreateRGBA8(RHIDevice &device, unsigned width, unsigned height,
                                                  const std::vector<unsigned char> &pixels,
                                                  TextureColorSpace space)
{
    if (!width || !height || width > device.Capabilities().maxTextureSize ||
        height > device.Capabilities().maxTextureSize ||
        static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() / height / 4 ||
        pixels.size() != static_cast<std::size_t>(width) * height * 4)
        return {};
    RHITextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.mipmaps = true;
    desc.format = space == TextureColorSpace::SRGB ? RHITextureFormat::SRGBA8 : RHITextureFormat::RGBA8;
    const auto texture = device.CreateTexture(desc, {pixels.data()});
    if (texture == 0)
        return {};
    return std::shared_ptr<Texture2D>(new Texture2D(device, texture, space));
}
void Texture2D::Bind(RHIDevice& consumer, unsigned unit) const
{
    if (!IsValid())
        throw RHIError("Texture belongs to a closed device");
    consumer.Commands().BindTexture(m_TextureID, unit);
}
