// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/InstanceBuffer.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "Engine/Rendering/Renderer/Resources/Shader.h"

InstanceBuffer::~InstanceBuffer()
{
    m_Device.DestroyBuffer(m_Buffer);
}

bool InstanceBuffer::Init()
{
    const unsigned offsetAlignment = m_Device.Capabilities().uniformBufferAlignment;
    const std::size_t blockSize =
        sizeof(InstanceTransformData) * MaxInstancesPerDraw;
    const std::size_t alignment = static_cast<std::size_t>(
        std::max(offsetAlignment, 1u));
    m_ChunkStride =
        ((blockSize + alignment - 1) / alignment) * alignment;

    return EnsureRegionCapacity(1);
}

bool InstanceBuffer::BindShaderBlock(const Shader& shader) const
{
    return m_Device.BindUniformBlock(shader.GetProgramID(), "InstanceTransforms", BindingPoint);
}

bool InstanceBuffer::UploadChunks(
    const InstanceTransformData* paddedChunks,
    std::size_t chunkCount)
{
    if (m_Buffer == 0 || paddedChunks == nullptr || chunkCount == 0 ||
        !EnsureRegionCapacity(chunkCount))
        return false;

    const std::size_t blockSize =
        sizeof(InstanceTransformData) * MaxInstancesPerDraw;
    const std::size_t uploadSize = chunkCount * m_ChunkStride;
    m_UploadBytes.assign(uploadSize, 0);
    for (std::size_t chunkIndex = 0;
        chunkIndex < chunkCount;
        ++chunkIndex)
    {
        std::memcpy(
            m_UploadBytes.data() + chunkIndex * m_ChunkStride,
            paddedChunks + chunkIndex * MaxInstancesPerDraw,
            blockSize);
    }

    m_CurrentRegion = (m_CurrentRegion + 1) % BufferedRegionCount;
    m_CurrentRegionOffset =
        m_CurrentRegion * m_RegionChunkCapacity * m_ChunkStride;

    m_Device.UpdateBuffer(m_Buffer, m_CurrentRegionOffset, uploadSize, m_UploadBytes.data());
    return true;
}

void InstanceBuffer::BindChunk(std::size_t chunkIndex) const
{
    if (m_Buffer == 0 || chunkIndex >= m_RegionChunkCapacity)
        return;

    const std::size_t blockSize =
        sizeof(InstanceTransformData) * MaxInstancesPerDraw;
    m_Device.Commands().BindUniformBuffer(m_Buffer, BindingPoint,
        m_CurrentRegionOffset + chunkIndex * m_ChunkStride, blockSize);
}

bool InstanceBuffer::EnsureRegionCapacity(
    std::size_t requiredChunkCount)
{
    if (m_ChunkStride == 0)
        return false;
    if (requiredChunkCount <= m_RegionChunkCapacity)
        return true;

    std::size_t newCapacity =
        std::max<std::size_t>(1, m_RegionChunkCapacity);
    while (newCapacity < requiredChunkCount)
        newCapacity *= 2;

    const auto candidate = m_Device.CreateBuffer({RHIBufferUsage::Uniform,
        BufferedRegionCount * newCapacity * m_ChunkStride, true});
    if (candidate == 0) return false;
    m_Device.DestroyBuffer(m_Buffer); m_Buffer = candidate;

    m_RegionChunkCapacity = newCapacity;
    m_CurrentRegion = BufferedRegionCount - 1;
    m_CurrentRegionOffset = 0;
    return true;
}
