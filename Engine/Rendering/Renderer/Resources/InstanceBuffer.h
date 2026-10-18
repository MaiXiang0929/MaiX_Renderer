// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <vector>

#include "Engine/Rendering/RHI/RHIDevice.h"

#include "Engine/Rendering/Renderer/View/InstanceTransformData.h"

class Shader;

class InstanceBuffer
{
public:
    static constexpr std::size_t MaxInstancesPerDraw = 256;
    static constexpr unsigned BindingPoint = 1;

    ~InstanceBuffer();

    InstanceBuffer(const InstanceBuffer&) = delete;
    InstanceBuffer& operator=(const InstanceBuffer&) = delete;
    explicit InstanceBuffer(RHIDevice& device) : m_Device(device) {}

    bool Init();
    bool BindShaderBlock(const Shader& shader) const;
    bool UploadChunks(
        const InstanceTransformData* paddedChunks,
        std::size_t chunkCount);
    void BindChunk(std::size_t chunkIndex) const;

private:
    RHIDevice& m_Device;
    static constexpr std::size_t BufferedRegionCount = 6;

    bool EnsureRegionCapacity(std::size_t requiredChunkCount);

    RHIBufferHandle m_Buffer = 0;
    std::size_t m_ChunkStride = 0;
    std::size_t m_RegionChunkCapacity = 0;
    std::size_t m_CurrentRegion = BufferedRegionCount - 1;
    std::size_t m_CurrentRegionOffset = 0;
    std::vector<unsigned char> m_UploadBytes;
};
