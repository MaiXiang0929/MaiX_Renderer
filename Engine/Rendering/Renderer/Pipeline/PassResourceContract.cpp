// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Pipeline/PassResourceContract.h"

#include "Engine/Rendering/Renderer/Pipeline/RenderTargetSizing.h"

namespace
{
std::size_t Index(PassResourceId id)
{
    const auto index = static_cast<std::size_t>(id);
    if (index >= RenderResourceCount)
        throw ResourceContractError("Invalid render resource ID.");
    return index;
}
bool IsImported(PassResourceId id)
{
    return id == PassResourceId::Environment || id == PassResourceId::Irradiance ||
           id == PassResourceId::PrefilteredEnvironment || id == PassResourceId::BrdfLut;
}
ResourceFormat ExpectedFormat(PassResourceId id)
{
    switch (id)
    {
    case PassResourceId::Environment: return ResourceFormat::Cubemap;
    case PassResourceId::Irradiance:
    case PassResourceId::PrefilteredEnvironment: return ResourceFormat::Cubemap16F;
    case PassResourceId::BrdfLut: return ResourceFormat::RGBA16F;
    case PassResourceId::ShadowDepth: return ResourceFormat::DepthArray;
    case PassResourceId::ShadowCubeDepth: return ResourceFormat::DepthCubeArray;
    case PassResourceId::ForwardDepth: return ResourceFormat::Depth24Stencil8;
    case PassResourceId::SsaoAO: return ResourceFormat::R8;
    case PassResourceId::ForwardIndirect:
    case PassResourceId::OutlinedIndirect:
    case PassResourceId::SceneIndirect:
    case PassResourceId::ForwardColor:
    case PassResourceId::ReflectionColor:
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
        "Environment", "Irradiance", "PrefilteredEnvironment", "BrdfLut",
        "ShadowDepth", "ReflectionColor", "ForwardColor", "ForwardDepth",
        "OutlinedColor", "SceneHdrColor", "SsaoAO", "SsaoColor", "Bloom",
        "PostColor", "Overlay", "FinalColor", "ForwardIndirect", "OutlinedIndirect", "SceneIndirect", "ShadowCubeDepth", "ShadowPreview"};
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
        read(Id::Irradiance); read(Id::PrefilteredEnvironment); read(Id::BrdfLut);
        if (features.shadows) { read(Id::ShadowDepth); read(Id::ShadowCubeDepth); }
    };
    switch (pass)
    {
    case RenderPassType::ShadowPreview:
        if (features.shadows && features.shadowPreview) {
            read(Id::ShadowDepth); read(Id::ShadowCubeDepth);
            write(Id::ShadowPreview, Access::ColorWrite, Load::Clear);
        }
        break;
    case RenderPassType::Shadow:
        if (features.shadows) {
            write(Id::ShadowDepth, Access::DepthWrite, Load::None, Load::Clear);
            write(Id::ShadowCubeDepth, Access::DepthWrite, Load::None, Load::Clear);
        }
        break;
    case RenderPassType::Reflection:
        surface();
        write(Id::ReflectionColor, Access::ColorWrite, Load::Clear, Load::Clear);
        break;
    case RenderPassType::Forward:
        surface(); read(Id::ReflectionColor);
        write(Id::ForwardColor, Access::ColorWrite, Load::Clear, Load::Clear);
        write(Id::ForwardIndirect, Access::ColorWrite, Load::Clear, Load::Clear);
        write(Id::ForwardDepth, Access::DepthWrite, Load::None, Load::Clear);
        break;
    case RenderPassType::Outline:
        read(Id::ForwardIndirect, Access::PreserveColor);
        write(Id::OutlinedIndirect, Access::ColorWrite, Load::Preserve, Load::Preserve, Id::ForwardIndirect);
        read(Id::ForwardColor, Access::PreserveColor); read(Id::ForwardDepth, Access::DepthTest);
        write(Id::OutlinedColor, Access::ColorWrite, Load::Preserve, Load::Preserve, Id::ForwardColor);
        break;
    case RenderPassType::Translucency:
        read(Id::OutlinedIndirect, Access::PreserveColor);
        write(Id::SceneIndirect, Access::ColorWrite, Load::Preserve, Load::Preserve, Id::OutlinedIndirect);
        surface(); read(Id::OutlinedColor, Access::PreserveColor); read(Id::ForwardDepth, Access::DepthTest);
        write(Id::SceneHdrColor, Access::ColorWrite, Load::Preserve, Load::Preserve, Id::OutlinedColor);
        break;
    case RenderPassType::SSAO:
        if (features.ssao)
        {
            read(Id::SceneHdrColor); read(Id::ForwardDepth); read(Id::SceneIndirect);
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
    for (std::size_t i=0;i<RenderResourceCount;++i)
        available[i] = IsImported(static_cast<PassResourceId>(i));
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
    m_WrittenLayers = {};
    m_Width = width;
    m_Height = height;
}
void FrameResources::Bind(PassResourceId id, RenderResourceBinding binding)
{
    const auto index = Index(id);
    if (binding.imported != IsImported(id))
        throw ResourceContractError("Only renderer-owned environment resources can be imported.");
    m_Bindings[index] = binding;
    m_WrittenLayers[index].assign(binding.requiredLayers, false);
    m_Produced[index] = binding.imported;
}
bool FrameResources::IsProduced(PassResourceId id) const { return m_Produced[Index(id)]; }
RHITextureHandle FrameResources::GetFinalTexture() const
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
    if (binding.format != ExpectedFormat(id) && !(id == PassResourceId::Environment && binding.format == ResourceFormat::Cubemap16F)) Fail(id, "has an incompatible format.");
    if (!binding.texture && !optional) Fail(id, "has a zero texture.");
    if (IsImported(id))
    {
        if (id != PassResourceId::Environment && (!binding.width || binding.width != binding.height))
            Fail(id, "has an invalid environment extent.");
        if (id == PassResourceId::PrefilteredEnvironment && !binding.mipmaps)
            Fail(id, "requires a roughness mip chain.");
        return;
    }
    if (!binding.framebuffer) Fail(id, "has no render target.");
    RenderTargetExtent expected{m_Frame.m_Width, m_Frame.m_Height};
    if (id == PassResourceId::ReflectionColor)
        expected = CalculateReflectionTargetExtent(m_Frame.m_Width, m_Frame.m_Height);
    else if (id == PassResourceId::ShadowPreview)
        expected = {256,256};
    else if (id == PassResourceId::Bloom || id == PassResourceId::SsaoAO)
        expected = CalculateBloomTargetExtent(m_Frame.m_Width, m_Frame.m_Height);
    else if (id == PassResourceId::ShadowDepth || id == PassResourceId::ShadowCubeDepth)
    {
        if (!binding.width || binding.width != binding.height || !binding.layers ||
            !binding.requiredLayers || binding.requiredLayers > binding.layers ||
            (id == PassResourceId::ShadowCubeDepth && binding.layers % 6 != 0))
            Fail(id, "has invalid shadow array dimensions or layers.");
        return;
    }
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
    const bool depth = id == PassResourceId::ShadowDepth || id == PassResourceId::ShadowCubeDepth || id == PassResourceId::ForwardDepth;
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
                (binding.texture == output.texture || binding.texture == output.depthTexture || binding.texture == output.otherColorTexture))
                Fail(declaration.id, "has a sampled input overlapping its target attachment.");
        }
    }
}
void PassResources::Publish(PassResourceId id)
{
    const auto& declaration = FindOutput(id);
    const auto& binding = Output(id);
    if (id == PassResourceId::ShadowDepth || id == PassResourceId::ShadowCubeDepth)
        for (bool written : m_Frame.m_WrittenLayers[Index(id)])
            if (!written) Fail(id, "contains an incomplete shadow layer.");
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
