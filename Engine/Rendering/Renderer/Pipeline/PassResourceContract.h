// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "Engine/Rendering/RHI/RHIDevice.h"

#include "Engine/Rendering/Renderer/Pipeline/RenderPassType.h"

// 阶段名表示内容已经执行到哪里；三个主颜色阶段借用同一个 FBO，不分配新纹理。
enum class PassResourceId
{
    Environment, Irradiance, PrefilteredEnvironment, BrdfLut,
    ShadowDepth, ReflectionColor, ForwardColor, ForwardDepth,
    OutlinedColor, SceneHdrColor, SsaoAO, SsaoColor, Bloom,
    PostColor, Overlay, FinalColor, ForwardIndirect, OutlinedIndirect, SceneIndirect, ShadowCubeDepth, ShadowPreview, Count
};
enum class ResourceFormat { RGBA8, RGBA16F, R8, Depth24, Depth24Stencil8, Cubemap, Cubemap16F, DepthArray, DepthCubeArray };
enum class ResourceAccess { Sample, PreserveColor, DepthTest, ColorWrite, DepthWrite };
enum class AttachmentLoad { None, Clear, Preserve };

constexpr std::size_t RenderResourceCount = static_cast<std::size_t>(PassResourceId::Count);
const char* RenderResourceName(PassResourceId id);

struct PassResourceFeatures
{
    bool shadows = true;
    bool ssao = false;
    bool bloom = true;
    bool shadowPreview = false;
};
struct ResourceInput
{
    PassResourceId id = PassResourceId::Count;
    ResourceAccess access = ResourceAccess::Sample;
    bool optional = false;
};
struct ResourceOutput
{
    PassResourceId id = PassResourceId::Count;
    ResourceAccess access = ResourceAccess::ColorWrite;
    AttachmentLoad colorLoad = AttachmentLoad::None;
    AttachmentLoad depthLoad = AttachmentLoad::None;
    PassResourceId preservedSource = PassResourceId::Count;
};
struct PassResourceContract
{
    RenderPassType pass = RenderPassType::Count;
    std::array<ResourceInput, 12> inputs{};
    std::array<ResourceOutput, 3> outputs{};
    std::size_t inputCount = 0;
    std::size_t outputCount = 0;
    PassResourceId hdrInput = PassResourceId::SceneHdrColor;
};

PassResourceContract BuildPassResourceContract(RenderPassType pass, PassResourceFeatures features);
bool ValidatePassResourceSequence(const PassResourceContract* contracts, std::size_t count,
                                 std::string& error);

// 纯 CPU 绑定快照；不拥有 GL 对象，也不跨帧缓存。depthTexture 用于检测附件反馈。
struct RenderResourceBinding
{
    RHITextureHandle texture = 0;
    RHITargetHandle framebuffer = 0;
    RHITextureHandle depthTexture = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    ResourceFormat format = ResourceFormat::RGBA8;
    bool hasDepth = false;
    bool imported = false;
    bool mipmaps = false;
    RHITextureHandle otherColorTexture = 0; // Sibling attachment for feedback validation.
    unsigned layers = 1;
    unsigned requiredLayers = 1;
};

class ResourceContractError : public RHIError
{
public:
    using RHIError::RHIError;
};

class PassResources;
class FrameResources
{
public:
    void BeginFrame(unsigned int width, unsigned int height);
    void Bind(PassResourceId id, RenderResourceBinding binding);
    bool IsProduced(PassResourceId id) const;
    RHITextureHandle GetFinalTexture() const;

private:
    friend class PassResources;
    std::array<RenderResourceBinding, RenderResourceCount> m_Bindings{};
    std::array<bool, RenderResourceCount> m_Produced{};
    std::array<std::vector<bool>, RenderResourceCount> m_WrittenLayers;
    unsigned int m_Width = 0;
    unsigned int m_Height = 0;
};

// 参考 Filament 的执行期资源访问检查：当前 Pass 只能取得它已声明的输入/输出。
class PassResources
{
public:
    PassResources(FrameResources& frame, const PassResourceContract& contract, RHICommandList* commands = nullptr)
        : m_Frame(frame), m_Contract(contract), m_Commands(commands) {}

    void Validate() const;
    const RenderResourceBinding& Read(PassResourceId id, ResourceAccess access = ResourceAccess::Sample) const;
    const RenderResourceBinding& Output(PassResourceId id) const;
    RHITextureHandle Texture(PassResourceId id) const { return Read(id).texture; }
    RHITextureHandle HdrTexture() const { return Texture(m_Contract.hdrInput); }
    void Publish(PassResourceId id);
    void Complete() const;

    // GL 操作集中在实现文件；清空/保留行为来自同一份输出声明。
    void BeginTarget(PassResourceId id) const;
    void EndTarget(PassResourceId id, bool generateMipmaps = true) const;
    void BeginLayer(PassResourceId id, unsigned layer);
    void EndLayer(PassResourceId id);

private:
    [[noreturn]] void Fail(PassResourceId id, const char* reason) const;
    void ValidateBinding(PassResourceId id, bool optional) const;
    const ResourceOutput& FindOutput(PassResourceId id) const;
    RHICommandList& Commands() const;
    FrameResources& m_Frame;
    const PassResourceContract& m_Contract;
    RHICommandList* m_Commands;
    std::array<bool, 3> m_Published{};
    PassResourceId m_ActiveLayerResource = PassResourceId::Count;
    unsigned m_ActiveLayer = 0;
};
