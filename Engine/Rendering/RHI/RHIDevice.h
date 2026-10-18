// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include "Engine/Rendering/RHI/RHITypes.h"

#include "Engine/Rendering/RHI/RHICommandList.h"

class RHIDevice
{
  public:
    virtual ~RHIDevice() = default;
    std::weak_ptr<void> Lifetime() const
    {
        return m_Lifetime;
    }
    virtual const RHICapabilities &Capabilities() const = 0;
    virtual RHICommandList &Commands() = 0;
    virtual RHITextureHandle CreateTexture(const RHITextureDesc &,
                                           const std::array<const void *, 6> &data = {}) = 0;
    virtual bool DestroyTexture(RHITextureHandle) = 0;
    virtual RHIBufferHandle CreateBuffer(const RHIBufferDesc &, const void *data = nullptr) = 0;
    virtual void UpdateBuffer(RHIBufferHandle, std::size_t offset, std::size_t size, const void *) = 0;
    virtual bool DestroyBuffer(RHIBufferHandle) = 0;
    virtual RHIVertexInputHandle CreateVertexInput(const RHIVertexInputDesc &) = 0;
    virtual bool DestroyVertexInput(RHIVertexInputHandle) = 0;
    virtual RHIProgramHandle CreateProgram(const std::vector<RHIShaderSource> &) = 0;
    virtual bool DestroyProgram(RHIProgramHandle) = 0;
    virtual bool BindUniformBlock(RHIProgramHandle, const char *name, unsigned slot) = 0;
    virtual RHITarget CreateTarget(const RHITargetDesc &) = 0;
    virtual bool DestroyTarget(RHITargetHandle) = 0;
    // Remap current bindings before replacing a target; attachments are not transferred.
    virtual void RemapTargetBindings(RHITargetHandle oldTarget, RHITargetHandle newTarget) = 0;
    virtual RHIQueryHandle CreateQuery() = 0;
    virtual bool DestroyQuery(RHIQueryHandle) = 0;
    virtual bool TryQueryResult(RHIQueryHandle, std::uint64_t &nanoseconds) = 0;

  private:
    // Shared textures may outlive their session as CPU objects. Device shutdown
    // releases the GPU records; their later destructors must not access this device.
    std::shared_ptr<void> m_Lifetime = std::make_shared<int>(0);
};

// One device per renderer/session, constructed with the host context current.
// All owned resources must be released before the device and host context.
std::unique_ptr<RHIDevice> CreateRHIDevice();

class RHIScopedPipelineState
{
  public:
    explicit RHIScopedPipelineState(RHICommandList &commands)
        : saved(commands.GetPipelineState()), m_Commands(commands)
    {
    }
    ~RHIScopedPipelineState()
    {
        m_Commands.SetPipelineState(saved);
    }
    RHIScopedPipelineState(const RHIScopedPipelineState &) = delete;
    RHIScopedPipelineState &operator=(const RHIScopedPipelineState &) = delete;
    const RHIPipelineState saved;

  private:
    RHICommandList &m_Commands;
};
