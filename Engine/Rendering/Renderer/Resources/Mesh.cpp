// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include <limits>
Mesh::~Mesh()
{
    Cleanup();
}
void Mesh::Cleanup()
{
    m_Device.DestroyVertexInput(m_VAO);
    m_Device.DestroyBuffer(m_EBO);
    m_Device.DestroyBuffer(m_VBO);
    m_VAO = 0;
    m_EBO = 0;
    m_VBO = 0;
    m_VertexCount = 0;
    m_IndexCount = 0;
}
void Mesh::Upload(const std::vector<Vertex> &vertices)
{
    Upload(vertices, {});
}
void Mesh::Upload(const std::vector<Vertex> &vertices, const std::vector<std::uint32_t> &indices)
{
    if (vertices.size() > INT_MAX || indices.size() > INT_MAX)
        throw std::length_error("Mesh exceeds draw range");
    for (auto index : indices)
        if (index >= vertices.size())
            throw std::out_of_range("Mesh index exceeds vertices");
    if (vertices.empty())
    {
        Cleanup();
        return;
    }
    Mesh candidate(m_Device);
    candidate.m_VBO =
        m_Device.CreateBuffer({RHIBufferUsage::Vertex, vertices.size() * sizeof(Vertex)}, vertices.data());
    if (candidate.m_VBO == 0)
        throw RHIError("Mesh vertex allocation failed");
    if (!indices.empty())
    {
        candidate.m_EBO = m_Device.CreateBuffer(
            {RHIBufferUsage::Index, indices.size() * sizeof(std::uint32_t)}, indices.data());
        if (candidate.m_EBO == 0)
            throw RHIError("Mesh index allocation failed");
    }
    candidate.m_VAO = m_Device.CreateVertexInput({candidate.m_VBO,
                                                  candidate.m_EBO,
                                                  sizeof(Vertex),
                                                  {{0, 3, offsetof(Vertex, Position)},
                                                   {1, 3, offsetof(Vertex, Normal)},
                                                   {2, 2, offsetof(Vertex, TexCoord)},
                                                   {3, 4, offsetof(Vertex, Tangent)}}});
    if (candidate.m_VAO == 0)
        throw RHIError("Mesh vertex input creation failed");
    Cleanup();
    std::swap(m_VAO, candidate.m_VAO);
    std::swap(m_VBO, candidate.m_VBO);
    std::swap(m_EBO, candidate.m_EBO);
    m_VertexCount = static_cast<int>(vertices.size());
    m_IndexCount = static_cast<int>(indices.size());
}
void Mesh::Draw() const
{
    if (m_VertexCount)
        m_Device.Commands().Draw(m_VAO, RHITopology::Triangles, m_IndexCount ? m_IndexCount : m_VertexCount,
                                 m_IndexCount > 0);
}
void Mesh::DrawInstanced(std::size_t count) const
{
    if (count > INT_MAX)
        throw std::length_error("Instance count exceeds draw range");
    if (m_VertexCount && count)
        m_Device.Commands().Draw(m_VAO, RHITopology::Triangles, m_IndexCount ? m_IndexCount : m_VertexCount,
                                 m_IndexCount > 0, static_cast<unsigned>(count));
}
void Mesh::DrawPatches() const
{
    if (m_VertexCount)
        m_Device.Commands().Draw(m_VAO, RHITopology::Patches, m_IndexCount ? m_IndexCount : m_VertexCount,
                                 m_IndexCount > 0);
}
int Mesh::GetVertexCount() const
{
    return m_VertexCount;
}
int Mesh::GetIndexCount() const
{
    return m_IndexCount;
}
