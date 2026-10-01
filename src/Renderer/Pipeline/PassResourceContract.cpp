// SPDX-License-Identifier: MIT
#include "PassResourceContract.h"

#include "RenderTargetSizing.h"

namespace
{
std::size_t Index(PassResourceId id)
{
    const auto index = static_cast<std::size_t>(id);
    if (index >= RenderResourceCount)
        throw ResourceContractError("Invalid render resource ID.");
    return index;
}
ResourceFormat ExpectedFormat(PassResourceId id)
{
    switch (id)
    {
    case PassResourceId::Environment: return ResourceFormat::Cubemap;
    case PassResourceId::ShadowDepth: return ResourceFormat::Depth24;
    case PassResourceId::ForwardDepth: return ResourceFormat::Depth24Stencil8;
    case PassResourceId::SsaoAO: return ResourceFormat::R8;
    case PassResourceId::ForwardColor:
    case PassResourceId::OutlinedColor:
    case PassResourceId::SceneHdrColor:
    case PassResourceId::SsaoColor:
    case PassResourceId::Bloom: return ResourceFormat::RGBA16F;
    default: return ResourceFormat::RGBA8;
    }
}
}

const char* RenderResourceName(PassResourceId id)
{
    static constexpr const char* names[] = {
        "Environment", "ShadowDepth", "ReflectionColor", "ForwardColor", "ForwardDepth",
        "OutlinedColor", "SceneHdrColor", "SsaoAO", "SsaoColor", "Bloom",
        "PostColor", "Overlay", "FinalColor"};
    return static_cast<std::size_t>(id) < RenderResourceCount ? names[Index(id)] : "Unknown";
}

PassResourceContract BuildPassResourceContract(RenderPassType pass, PassResourceFeatures features)
{
    using Id = PassResourceId;
    using Access = ResourceAccess;
    using Load = AttachmentLoad;
    PassResourceContract result;
    result.pass = pass;
    result.hdrInput = features.ssao ? Id::SsaoColor : Id::SceneHdrColor;
    const auto read = [&result](Id id, Access access = Access::Sample, bool optional = false) {
        result.inputs[result.inputCount++] = {id, access, optional};
    };
    const auto write = [&result](Id id, Access access, Load color, Load depth = Load::None,
                                 Id source = Id::Count) {
        result.outputs[result.outputCount++] = {id, access, color, depth, source};
    };
    const auto surface = [&] {
        read(Id::Environment, Access::Sample, true);
        if (features.shadows) read(Id::ShadowDepth);
    };
    switch (pass)
    {
    case RenderPassType::Shadow:
        if (features.shadows) write(Id::ShadowDepth, Access::DepthWrite, Load::None, Load::Clear);
        break;
    case RenderPassType::Reflection:
        surface();
        write(Id::ReflectionColor, Access::ColorWrite, Load::Clear, Load::Clear);
        break;
    case RenderPassType::Forward:
        surface(); read(Id::ReflectionColor);
        write(Id::ForwardColor, Access::ColorWrite, Load::Clear, Load::Clear);
        write(Id::ForwardDepth, Access::DepthWrite, Load::None, Load::Clear);
        break;
    case RenderPassType::Outline:
        read(Id::ForwardColor, Access::PreserveColor); read(Id::ForwardDepth, Access::DepthTest);
        write(Id::OutlinedColor, Access::ColorWrite, Load::Preserve, Load::Preserve, Id::ForwardColor);
        break;
    case RenderPassType::Translucency:
        surface(); read(Id::OutlinedColor, Access::PreserveColor); read(Id::ForwardDepth, Access::DepthTest);
        write(Id::SceneHdrColor, Access::ColorWrite, Load::Preserve, Load::Preserve, Id::OutlinedColor);
        break;
    case RenderPassType::SSAO:
        if (features.ssao)
        {
            read(Id::SceneHdrColor); read(Id::ForwardDepth);
            write(Id::SsaoAO, Access::ColorWrite, Load::Clear);
            write(Id::SsaoColor, Access::ColorWrite, Load::Clear);
        }
        break;
    case RenderPassType::Bloom:
        if (features.bloom)
        {
            read(result.hdrInput);
            write(Id::Bloom, Access::ColorWrite, Load::None);
        }
        break;
    case RenderPassType::PostProcess:
        read(result.hdrInput);
        if (features.bloom) read(Id::Bloom);
        write(Id::PostColor, Access::ColorWrite, Load::Clear);
        break;
    case RenderPassType::EditorPrimitive:
        write(Id::Overlay, Access::ColorWrite, Load::Clear);
        break;
    case RenderPassType::Present:
        read(Id::PostColor); read(Id::Overlay);
        write(Id::FinalColor, Access::ColorWrite, Load::Clear);
        break;
    case RenderPassType::Count:
        throw ResourceContractError("Invalid render pass type.");
    }
    return result;
}

bool ValidatePassResourceSequence(const PassResourceContract* contracts, std::size_t count,
                                 std::string& error)
{
    std::array<bool, RenderResourceCount> available{};
    available[Index(PassResourceId::Environment)] = true; // Renderer 注入的外部资源。
    for (std::size_t pass = 0; pass < count; ++pass)
    {
        const auto& contract = contracts[pass];
        for (std::size_t i = 0; i < contract.inputCount; ++i)
            if (!available[Index(contract.inputs[i].id)])
            {
                error = std::string(RenderPassName(contract.pass)) + ": " +
                    RenderResourceName(contract.inputs[i].id) + " has no preceding producer.";
                return false;
            }
        for (std::size_t i = 0; i < contract.outputCount; ++i)
        {
            const auto& output = contract.outputs[i];
            if (output.colorLoad == AttachmentLoad::Preserve)
            {
                bool declared = false;
                for (std::size_t input = 0; input < contract.inputCount; ++input)
                    declared |= contract.inputs[input].id == output.preservedSource &&
                        contract.inputs[input].access == ResourceAccess::PreserveColor;
                if (!declared || !available[Index(output.preservedSource)])
                {
                    error = std::string(RenderPassName(contract.pass)) + ": " +
                        RenderResourceName(output.id) + " has no declared preserved source.";
                    return false;
                }
            }
            if (available[Index(output.id)])
            {
                error = std::string(RenderPassName(contract.pass)) + ": duplicate producer for " +
                    RenderResourceName(output.id);
                return false;
            }
            available[Index(output.id)] = true;
        }
    }
    if (!available[Index(PassResourceId::FinalColor)])
    {
        error = "Present: FinalColor has no producer.";
        return false;
    }
    error.clear();
    return true;
}

void FrameResources::BeginFrame(unsigned int width, unsigned int height)
{
    m_Bindings = {};
    m_Produced = {};
    m_Width = width;
    m_Height = height;
}
void FrameResources::Bind(PassResourceId id, RenderResourceBinding binding)
{
    const auto index = Index(id);
    if (binding.imported != (id == PassResourceId::Environment))
        throw ResourceContractError("Only Environment can be imported in this pipeline.");
    m_Bindings[index] = binding;
    m_Produced[index] = binding.imported;
}
bool FrameResources::IsProduced(PassResourceId id) const { return m_Produced[Index(id)]; }
std::uint32_t FrameResources::GetFinalTexture() const
{
    return IsProduced(PassResourceId::FinalColor)
        ? m_Bindings[Index(PassResourceId::FinalColor)].texture : 0;
}

[[noreturn]] void PassResources::Fail(PassResourceId id, const char* reason) const
{
    throw ResourceContractError(std::string(RenderPassName(m_Contract.pass)) + ": " +
        RenderResourceName(id) + " " + reason);
}
void PassResources::ValidateBinding(PassResourceId id, bool optional) const
{
    const auto& binding = m_Frame.m_Bindings[Index(id)];
    if (binding.format != ExpectedFormat(id)) Fail(id, "has an incompatible format.");
    if (!binding.texture && !optional) Fail(id, "has a zero texture.");
    if (id == PassResourceId::Environment) return;
    if (!binding.framebuffer) Fail(id, "has no render target.");
    RenderTargetExtent expected{m_Frame.m_Width, m_Frame.m_Height};
    if (id == PassResourceId::ReflectionColor)
        expected = CalculateReflectionTargetExtent(m_Frame.m_Width, m_Frame.m_Height);
    else if (id == PassResourceId::Bloom || id == PassResourceId::SsaoAO)
        expected = CalculateBloomTargetExtent(m_Frame.m_Width, m_Frame.m_Height);
    else if (id == PassResourceId::ShadowDepth)
        expected = {2048, 2048};
    if (!expected.width || !expected.height || binding.width != expected.width || binding.height != expected.height)
        Fail(id, "has an incompatible extent.");
}
const RenderResourceBinding& PassResources::Read(PassResourceId id, ResourceAccess access) const
{
    for (std::size_t i = 0; i < m_Contract.inputCount; ++i)
    {
        const auto& input = m_Contract.inputs[i];
        if (input.id != id || input.access != access) continue;
        ValidateBinding(id, input.optional);
        if (!m_Frame.IsProduced(id)) Fail(id, "has not been produced in this frame.");
        return m_Frame.m_Bindings[Index(id)];
    }
    Fail(id, "access was not declared.");
}
const ResourceOutput& PassResources::FindOutput(PassResourceId id) const
{
    for (std::size_t i = 0; i < m_Contract.outputCount; ++i)
        if (m_Contract.outputs[i].id == id) return m_Contract.outputs[i];
    Fail(id, "output was not declared.");
}
const RenderResourceBinding& PassResources::Output(PassResourceId id) const
{
    const auto& declaration = FindOutput(id);
    ValidateBinding(id, false);
    const bool depth = id == PassResourceId::ShadowDepth || id == PassResourceId::ForwardDepth;
    if (declaration.access != (depth ? ResourceAccess::DepthWrite : ResourceAccess::ColorWrite))
        Fail(id, "has an incompatible output access.");
    return m_Frame.m_Bindings[Index(id)];
}
void PassResources::Validate() const
{
    for (std::size_t i = 0; i < m_Contract.inputCount; ++i)
        Read(m_Contract.inputs[i].id, m_Contract.inputs[i].access);
    for (std::size_t i = 0; i < m_Contract.outputCount; ++i)
    {
        const auto& declaration = m_Contract.outputs[i];
        const auto& output = Output(declaration.id);
        if (declaration.depthLoad != AttachmentLoad::None && !output.hasDepth)
            Fail(declaration.id, "requires a depth attachment.");
        if (declaration.access == ResourceAccess::ColorWrite && output.hasDepth)
            for (std::size_t other = 0; other < m_Contract.outputCount; ++other)
                if (m_Contract.outputs[other].access == ResourceAccess::DepthWrite)
                {
                    const auto& depth = Output(m_Contract.outputs[other].id);
                    if (depth.framebuffer != output.framebuffer || depth.texture != output.depthTexture)
                        Fail(declaration.id, "does not use its declared output depth attachment.");
                }
        if (declaration.colorLoad == AttachmentLoad::Preserve)
        {
            const auto& source = Read(declaration.preservedSource, ResourceAccess::PreserveColor);
            if (source.texture != output.texture || source.framebuffer != output.framebuffer)
                Fail(declaration.id, "does not preserve the declared source target.");
        }
        for (std::size_t input = 0; input < m_Contract.inputCount; ++input)
        {
            const auto& use = m_Contract.inputs[input];
            const auto& binding = m_Frame.m_Bindings[Index(use.id)];
            if (use.access == ResourceAccess::DepthTest && declaration.depthLoad == AttachmentLoad::Preserve &&
                (!binding.hasDepth || binding.framebuffer != output.framebuffer || binding.depthTexture != output.depthTexture))
                Fail(declaration.id, "does not preserve the declared depth target.");
            if (use.access == ResourceAccess::Sample && binding.texture &&
                (binding.texture == output.texture || binding.texture == output.depthTexture))
                Fail(declaration.id, "has a sampled input overlapping its target attachment.");
        }
    }
}
void PassResources::Publish(PassResourceId id)
{
    const auto& declaration = FindOutput(id);
    const auto& binding = Output(id);
    if (declaration.colorLoad == AttachmentLoad::Preserve)
        Read(declaration.preservedSource, ResourceAccess::PreserveColor);
    // 更新同一物理颜色的阶段后，旧内容身份失效；这不是额外的 GPU 内存分配。
    for (std::size_t i = 0; i < RenderResourceCount; ++i)
        if (m_Frame.m_Bindings[i].texture == binding.texture &&
            m_Frame.m_Bindings[i].framebuffer == binding.framebuffer)
            m_Frame.m_Produced[i] = false;
    m_Frame.m_Produced[Index(id)] = true;
    for (std::size_t i = 0; i < m_Contract.outputCount; ++i)
        if (m_Contract.outputs[i].id == id) m_Published[i] = true;
}
void PassResources::Complete() const
{
    for (std::size_t i = 0; i < m_Contract.outputCount; ++i)
        if (!m_Published[i]) Fail(m_Contract.outputs[i].id, "was not published by its producer.");
}
