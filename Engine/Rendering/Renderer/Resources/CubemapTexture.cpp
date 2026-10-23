// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/CubemapTexture.h"
#include "lodepng.h"
#include <vector>
#include <iostream>
#include "Engine/Asset/Importer/ImageLoader.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
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
    m_IsHdr = false;
    release.texture = 0;
    return true;
}
void CubemapTexture::Bind(unsigned unit) const
{
    m_Device.Commands().BindTexture(m_TextureID, unit, RHITextureKind::Cubemap);
}


bool CubemapTexture::LoadHdr(const std::filesystem::path& path, std::string& error)
{
    AssetImport::HdrImageData image;
    if (!AssetImport::ImageLoader::LoadHdrFromFile(path, image, error)) return false;
    auto& commands = m_Device.Commands();
    RHIScopedPipelineState stateScope(commands);
    struct Pending
    {
        RHIDevice& device;
        RHITextureHandle panorama, cube;
        ~Pending()
        {
            device.Commands().EndTarget();
            device.Commands().BindProgram(0);
            device.Commands().InvalidateState();
            device.DestroyTexture(panorama); device.DestroyTexture(cube);
        }
    } pending{m_Device, {}, {}};
    try
    {
        RHITextureDesc texture;
        texture.width = image.width; texture.height = image.height;
        texture.format = RHITextureFormat::RGBA16F; texture.wrap = RHIWrap::ClampEdge;
        pending.panorama = m_Device.CreateTexture(texture, {image.pixels.data()});
        if (!pending.panorama) { error = "Cannot allocate HDR panorama."; return false; }
        unsigned size = 1;
        while (size < image.width / 4 && size < 1024) size *= 2;
        texture.width = texture.height = size;
        texture.kind = RHITextureKind::Cubemap; texture.mipmaps = true;
        pending.cube = m_Device.CreateTexture(texture);
        if (!pending.cube) { error = "Cannot allocate HDR cubemap."; return false; }
        Shader convert(m_Device);
        if (!convert.Load("Shaders/postprocess/fullscreen.vert", "Shaders/environment/equirectangular_to_cubemap.frag"))
        { error = "Cannot compile HDR conversion shader."; return false; }
        std::vector<Vertex> vertices(3);
        vertices[0].Position = {-1,-1,0}; vertices[0].TexCoord = {0,0};
        vertices[1].Position = {3,-1,0}; vertices[1].TexCoord = {2,0};
        vertices[2].Position = {-1,3,0}; vertices[2].TexCoord = {0,2};
        Mesh quad(m_Device); quad.Upload(vertices);
        Framebuffer scratch(m_Device); FramebufferSpecification spec;
        spec.width = spec.height = size; spec.colorFormat = FramebufferColorFormat::RGBA16F;
        spec.depthStencilEnabled = false; spec.mipmapsEnabled = false;
        if (!scratch.Init(spec)) { error = "Cannot allocate HDR conversion target."; return false; }
        RHIPipelineState state; state.depthTest = false; state.depthWrite = false;
        commands.SetPipelineState(state);
        convert.Bind(); convert.SetInt("panorama", 0); commands.BindTexture(pending.panorama, 0);
        for (unsigned face = 0; face < 6; ++face)
        {
            scratch.Bind(); convert.SetInt("face", static_cast<int>(face)); quad.Draw();
            commands.CopyTargetToCubemap(scratch.GetFramebufferId(), pending.cube, face, 0);
        }
        commands.EndTarget(); commands.GenerateMipmaps(pending.cube);
        if (!m_Lighting.Build(pending.cube, size)) { error = "HDR IBL generation failed."; return false; }
        // All candidates are ready. Commit uses no allocations and keeps CPU settings outside this object.
        std::swap(m_TextureID, pending.cube); m_Size = size; m_IsHdr = true;
        error.clear(); return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}
