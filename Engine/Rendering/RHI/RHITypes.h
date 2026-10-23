// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// IDs are globally unique and never reused. Zero is the only null value.
// The device validates ownership/liveness before resolving any native object.
template <class Tag> struct RHIHandle
{
    std::uint64_t id = 0;
    constexpr RHIHandle(std::uint64_t value = 0) : id(value) {}
    explicit constexpr operator bool() const
    {
        return id != 0;
    }
    friend constexpr bool operator==(RHIHandle a, RHIHandle b)
    {
        return a.id == b.id;
    }
    friend constexpr bool operator!=(RHIHandle a, RHIHandle b)
    {
        return !(a == b);
    }
};
using RHITextureHandle = RHIHandle<struct TextureTag>;
using RHIBufferHandle = RHIHandle<struct BufferTag>;
using RHIProgramHandle = RHIHandle<struct ProgramTag>;
using RHITargetHandle = RHIHandle<struct TargetTag>;
using RHIVertexInputHandle = RHIHandle<struct VertexInputTag>;
using RHIQueryHandle = RHIHandle<struct QueryTag>;
class RHIError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

enum class RHITextureKind
{
    Texture2D,
    Cubemap
};
enum class RHITextureFormat
{
    RGBA8,
    SRGBA8,
    RGBA16F,
    R8,
    Depth24,
    Depth24Stencil8
};
enum class RHIWrap
{
    Repeat,
    ClampEdge,
    ClampBorder
};
enum class RHIBufferUsage
{
    Vertex,
    Index,
    Uniform
};
enum class RHIShaderStage
{
    Vertex,
    Fragment,
    TessControl,
    TessEvaluation,
    Geometry
};
enum class RHITopology
{
    Triangles,
    Lines,
    Patches
};
enum class RHICompare
{
    Less,
    LessEqual,
    Always
};
enum class RHICull
{
    None,
    Front,
    Back
};
enum class RHIBlendFactor
{
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha
};
struct RHITextureDesc
{
    unsigned width = 0, height = 0;
    RHITextureKind kind = RHITextureKind::Texture2D;
    RHITextureFormat format = RHITextureFormat::RGBA8;
    RHIWrap wrap = RHIWrap::Repeat;
    bool mipmaps = false, nearest = false, compareDepth = false, anisotropic = false;
};
struct RHIBufferDesc
{
    RHIBufferUsage usage;
    std::size_t size;
    bool dynamic = false;
};
struct RHIShaderSource
{
    RHIShaderStage stage;
    std::string source;
    std::string label;
};
struct RHIVertexAttribute
{
    unsigned location, components;
    std::size_t offset;
};
struct RHIVertexInputDesc
{
    RHIBufferHandle vertices, indices;
    std::size_t stride = 0;
    std::vector<RHIVertexAttribute> attributes;
};
struct RHITargetDesc
{
    unsigned width = 0, height = 0;
    RHITextureFormat colorFormat = RHITextureFormat::RGBA8;
    bool color = true, depth = true, sampleableDepth = false, mipmaps = false;
    bool shadow = false;
    bool secondaryColor = false; // Optional second color attachment, same format, no mipmaps.
};
// Target exclusively owns its attachments. Returned texture handles are borrowed.
struct RHITarget
{
    RHITargetHandle handle;
    RHITextureHandle color, depth, secondaryColor;
};
struct RHIPipelineState
{
    bool seamlessCubemap = true;
    bool depthTest = true, depthWrite = true, blend = false, wireframe = false;
    RHICompare depthCompare = RHICompare::Less;
    RHICull cull = RHICull::None;
    RHIBlendFactor sourceRgb = RHIBlendFactor::One, destinationRgb = RHIBlendFactor::Zero;
    RHIBlendFactor sourceAlpha = RHIBlendFactor::One, destinationAlpha = RHIBlendFactor::Zero;
    bool offsetFill = false, offsetLine = false;
    float offsetFactor = 0, offsetUnits = 0;
};
struct RHICapabilities
{
    unsigned maxTextureSize = 0, uniformBufferAlignment = 1, maxTextureUnits = 0;
    std::size_t maxUniformBlockSize = 0;
    bool timerQueries = false, debugGroups = false;
};
