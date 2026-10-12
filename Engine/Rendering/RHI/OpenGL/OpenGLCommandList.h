// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/RHI/RHICommandList.h"
#include <cstdint>

// Explicit borrowed bridge for the existing PassResourceContract GL bindings.
// The texture remains owned by its pass and is valid only during execution.
class OpenGLTextureView final : public RHITextureView
{
public:
    explicit OpenGLTextureView(std::uint32_t texture) : m_Texture(texture) {}
    void Bind(unsigned int slot) const override;
private:
    std::uint32_t m_Texture;
};

class OpenGLCommandList final : public RHICommandList
{
public:
    void Composite(RHIPipeline& pipeline, const RHITextureView& scene,
        const RHITextureView& overlay, const RHIBuffer& geometry) override;
};
