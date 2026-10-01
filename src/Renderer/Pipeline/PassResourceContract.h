// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "RenderPassType.h"

// 阶段名表示内容已经执行到哪里；三个主颜色阶段借用同一个 FBO，不分配新纹理。
enum class PassResourceId
{
    Environment, ShadowDepth, ReflectionColor, ForwardColor, ForwardDepth,
    OutlinedColor, SceneHdrColor, SsaoAO, SsaoColor, Bloom,
    PostColor, Overlay, FinalColor, Count
};
enum class ResourceFormat { RGBA8, RGBA16F, R8, Depth24, Depth24Stencil8, Cubemap };
enum class ResourceAccess { Sample, PreserveColor, DepthTest, ColorWrite, DepthWrite };
enum class AttachmentLoad { None, Clear, Preserve };

constexpr std::size_t RenderResourceCount = static_cast<std::size_t>(PassResourceId::Count);
const char* RenderResourceName(PassResourceId id);

struct PassResourceFeatures
{
    bool shadows = true;
    bool ssao = false;
    bool bloom = true;
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
    std::array<ResourceInput, 6> inputs{};
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
    std::uint32_t texture = 0;
    std::uint32_t framebuffer = 0;
    std::uint32_t depthTexture = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    ResourceFormat format = ResourceFormat::RGBA8;
    bool hasDepth = false;
    bool imported = false;
    bool mipmaps = false;
};

class ResourceContractError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class PassResources;
class FrameResources
{
public:
    void BeginFrame(unsigned int width, unsigned int height);
    void Bind(PassResourceId id, RenderResourceBinding binding);
    bool IsProduced(PassResourceId id) const;
    std::uint32_t GetFinalTexture() const;

private:
    friend class PassResources;
    std::array<RenderResourceBinding, RenderResourceCount> m_Bindings{};
    std::array<bool, RenderResourceCount> m_Produced{};
    unsigned int m_Width = 0;
    unsigned int m_Height = 0;
};

// 参考 Filament 的执行期资源访问检查：当前 Pass 只能取得它已声明的输入/输出。
class PassResources
{
public:
    PassResources(FrameResources& frame, const PassResourceContract& contract)
        : m_Frame(frame), m_Contract(contract) {}

    void Validate() const;
    const RenderResourceBinding& Read(PassResourceId id, ResourceAccess access = ResourceAccess::Sample) const;
    const RenderResourceBinding& Output(PassResourceId id) const;
    std::uint32_t Texture(PassResourceId id) const { return Read(id).texture; }
    std::uint32_t HdrTexture() const { return Texture(m_Contract.hdrInput); }
    void Publish(PassResourceId id);
    void Complete() const;

    // GL 操作集中在实现文件；清空/保留行为来自同一份输出声明。
    void BeginTarget(PassResourceId id) const;
    void EndTarget(PassResourceId id, bool generateMipmaps = true) const;

private:
    [[noreturn]] void Fail(PassResourceId id, const char* reason) const;
    void ValidateBinding(PassResourceId id, bool optional) const;
    const ResourceOutput& FindOutput(PassResourceId id) const;
    FrameResources& m_Frame;
    const PassResourceContract& m_Contract;
    std::array<bool, 3> m_Published{};
};
