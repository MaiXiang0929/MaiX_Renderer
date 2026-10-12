// SPDX-License-Identifier: MIT
#pragma once
#include <string>

// First RHI slice: immediate fullscreen composition. Resource creation and
// the remaining passes retain their current wrappers until Phase 1.
class RHIPipeline
{
public:
    virtual ~RHIPipeline() = default;
    virtual void Bind() const = 0;
    virtual void SetInt(const std::string& name, int value) = 0;
};

class RHITextureView
{
public:
    virtual ~RHITextureView() = default;
    virtual void Bind(unsigned int slot) const = 0;
};

class RHIBuffer
{
public:
    virtual ~RHIBuffer() = default;
    virtual void Draw() const = 0;
};

class RHICommandList
{
public:
    virtual ~RHICommandList() = default;
    virtual void Composite(RHIPipeline& pipeline, const RHITextureView& scene,
        const RHITextureView& overlay, const RHIBuffer& geometry) = 0;
};
