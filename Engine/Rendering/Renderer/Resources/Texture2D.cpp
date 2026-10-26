// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/Texture2D.h"
#include "lodepng.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

void Texture2D::SetSource(std::filesystem::path path, std::uint64_t embeddedId)
{
    if (path.empty())
    {
        m_SourcePath.clear();
        m_EmbeddedId = 0;
        m_ReconstructionSource.reset();
        return;
    }
    AssetImport::TextureSource source;
    source.image = {std::move(path), embeddedId};
    SetReconstructionSource(source);
}

void Texture2D::SetReconstructionSource(const AssetImport::TextureSource& source)
{
    std::string error;
    if (!AssetImport::ValidateTextureSource(source, error))
        throw std::invalid_argument("Invalid texture reconstruction source: " + error);
    // Stage all allocating copies before replacing the current provenance.
    AssetImport::TextureSource candidate = source;
    std::filesystem::path directPath = source.kind == AssetImport::TextureSourceKind::Orm
        ? std::filesystem::path{} : source.image.path;
    const auto embeddedId = source.kind == AssetImport::TextureSourceKind::Orm ? 0 : source.image.embeddedId;
    m_ReconstructionSource = std::move(candidate);
    m_SourcePath = std::move(directPath);
    m_EmbeddedId = embeddedId;
}
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
    auto result=std::shared_ptr<Texture2D>(new Texture2D(device, texture, space));
    for(unsigned w=width,h=height;;w=std::max(1u,w/2),h=std::max(1u,h/2)) {
        result->m_EstimatedBytes+=static_cast<std::size_t>(w)*h*4;
        if(w==1&&h==1) break;
    }
    return result;
}
void Texture2D::Bind(RHIDevice& consumer, unsigned unit) const
{
    if (!IsValid())
        throw RHIError("Texture belongs to a closed device");
    consumer.Commands().BindTexture(m_TextureID, unit);
}
