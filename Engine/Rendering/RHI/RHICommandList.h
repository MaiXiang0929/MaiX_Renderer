// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/RHI/RHITypes.h"
class RHICommandList
{
  public:
    virtual ~RHICommandList() = default;
    virtual void BeginTarget(RHITargetHandle, bool clearColor, bool clearDepth, float alpha) = 0;
    virtual void EndTarget() = 0;
    virtual void GenerateMipmaps(RHITextureHandle) = 0;
    // Copy a color target to one existing cubemap face/mip; extents and formats must match.
    virtual void CopyTargetToCubemap(RHITargetHandle, RHITextureHandle, unsigned face, unsigned mip) = 0;
    virtual void BindTexture(RHITextureHandle, unsigned slot, RHITextureKind = RHITextureKind::Texture2D) = 0;
    virtual void BindProgram(RHIProgramHandle) = 0;
    virtual void SetInt(RHIProgramHandle, const std::string &, int) = 0;
    virtual void SetFloats(RHIProgramHandle, const std::string &, const float *, unsigned count) = 0;
    virtual void BindUniformBuffer(RHIBufferHandle, unsigned slot, std::size_t offset, std::size_t size) = 0;
    virtual void Draw(RHIVertexInputHandle, RHITopology, unsigned count, bool indexed,
                      unsigned instances = 1) = 0;
    virtual RHIPipelineState GetPipelineState() const = 0;
    virtual void SetPipelineState(const RHIPipelineState &) = 0;
    virtual void InvalidateState() = 0;
    virtual void BeginQuery(RHIQueryHandle) = 0;
    virtual void EndQuery() = 0;
    virtual bool PushDebugGroup(const char *) = 0;
    virtual void PopDebugGroup() = 0;
};
