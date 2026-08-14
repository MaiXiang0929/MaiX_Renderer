// SPDX-License-Identifier: MIT
#include "InstanceBuffer.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "Renderer/Resources/Shader.h"

InstanceBuffer::~InstanceBuffer()
{
    if (m_Buffer != 0)
        glDeleteBuffers(1, &m_Buffer);
}

bool InstanceBuffer::Init()
{
    GLint offsetAlignment = 1;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offsetAlignment);
    const std::size_t blockSize =
        sizeof(InstanceTransformData) * MaxInstancesPerDraw;
    const std::size_t alignment = static_cast<std::size_t>(
        std::max(offsetAlignment, 1));
    m_ChunkStride =
        ((blockSize + alignment - 1) / alignment) * alignment;

    glGenBuffers(1, &m_Buffer);
    return m_Buffer != 0 && EnsureRegionCapacity(1);
}

bool InstanceBuffer::BindShaderBlock(const Shader& shader) const
{
    const GLuint blockIndex = glGetUniformBlockIndex(
        shader.GetProgramID(), "InstanceTransforms");
    if (blockIndex == GL_INVALID_INDEX)
    {
        std::cerr
            << "[InstanceBuffer] InstanceTransforms uniform block was not found."
            << std::endl;
        return false;
    }
    glUniformBlockBinding(
        shader.GetProgramID(), blockIndex, BindingPoint);
    return true;
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

    glBindBuffer(GL_UNIFORM_BUFFER, m_Buffer);
    glBufferSubData(
        GL_UNIFORM_BUFFER,
        static_cast<GLintptr>(m_CurrentRegionOffset),
        static_cast<GLsizeiptr>(uploadSize),
        m_UploadBytes.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void InstanceBuffer::BindChunk(std::size_t chunkIndex) const
{
    if (m_Buffer == 0 || chunkIndex >= m_RegionChunkCapacity)
        return;

    const std::size_t blockSize =
        sizeof(InstanceTransformData) * MaxInstancesPerDraw;
    glBindBufferRange(
        GL_UNIFORM_BUFFER,
        BindingPoint,
        m_Buffer,
        static_cast<GLintptr>(
            m_CurrentRegionOffset + chunkIndex * m_ChunkStride),
        static_cast<GLsizeiptr>(blockSize));
}

bool InstanceBuffer::EnsureRegionCapacity(
    std::size_t requiredChunkCount)
{
    if (m_Buffer == 0 || m_ChunkStride == 0)
        return false;
    if (requiredChunkCount <= m_RegionChunkCapacity)
        return true;

    std::size_t newCapacity =
        std::max<std::size_t>(1, m_RegionChunkCapacity);
    while (newCapacity < requiredChunkCount)
        newCapacity *= 2;

    glBindBuffer(GL_UNIFORM_BUFFER, m_Buffer);
    glBufferData(
        GL_UNIFORM_BUFFER,
        static_cast<GLsizeiptr>(
            BufferedRegionCount * newCapacity * m_ChunkStride),
        nullptr,
        GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    m_RegionChunkCapacity = newCapacity;
    m_CurrentRegion = BufferedRegionCount - 1;
    m_CurrentRegionOffset = 0;
    return true;
}
