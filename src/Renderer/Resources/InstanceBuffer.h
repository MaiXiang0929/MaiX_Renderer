// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <vector>

#include <glad/glad.h>

#include "Renderer/View/InstanceTransformData.h"

class Shader;

class InstanceBuffer
{
public:
    static constexpr std::size_t MaxInstancesPerDraw = 256;
    static constexpr GLuint BindingPoint = 1;

    ~InstanceBuffer();

    InstanceBuffer(const InstanceBuffer&) = delete;
    InstanceBuffer& operator=(const InstanceBuffer&) = delete;
    InstanceBuffer() = default;

    bool Init();
    bool BindShaderBlock(const Shader& shader) const;
    bool UploadChunks(
        const InstanceTransformData* paddedChunks,
        std::size_t chunkCount);
    void BindChunk(std::size_t chunkIndex) const;

private:
    static constexpr std::size_t BufferedRegionCount = 6;

    bool EnsureRegionCapacity(std::size_t requiredChunkCount);

    GLuint m_Buffer = 0;
    std::size_t m_ChunkStride = 0;
    std::size_t m_RegionChunkCapacity = 0;
    std::size_t m_CurrentRegion = BufferedRegionCount - 1;
    std::size_t m_CurrentRegionOffset = 0;
    std::vector<unsigned char> m_UploadBytes;
};
