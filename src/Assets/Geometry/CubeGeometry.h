// SPDX-License-Identifier: MIT
#pragma once
#include "Renderer/Resources/Mesh.h"
#include <vector>
struct CubeGeometry
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};
// Flat face normals and separate UV seams require 24 rendering vertices.
inline CubeGeometry CreateCubeGeometry(float edgeLength)
{
    CubeGeometry result;
    const float h = edgeLength * 0.5f;
    const cy::Vec3f normals[] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                 {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    const cy::Vec3f tangents[] = {{0, 0, -1}, {0, 0, 1}, {1, 0, 0},
                                  {1, 0, 0},  {1, 0, 0}, {-1, 0, 0}};
    const cy::Vec2f uvs[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    result.vertices.reserve(24);
    result.indices.reserve(36);
    for (unsigned int face = 0; face < 6; ++face)
    {
        const auto n = normals[face], t = tangents[face], b = n.Cross(t);
        for (auto uv : uvs)
            result.vertices.push_back({n * h + t * ((uv.x * 2 - 1) * h) + b * ((uv.y * 2 - 1) * h),
                                       n,
                                       uv,
                                       {t.x, t.y, t.z, 1}});
        for (unsigned int index : {0u, 1u, 2u, 0u, 2u, 3u})
            result.indices.push_back(face * 4 + index);
    }
    return result;
}
