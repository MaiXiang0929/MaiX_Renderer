// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/View/Frustum.h"

#include <cmath>

namespace
{
FrustumPlane MakePlane(float x, float y, float z, float distance)
{
    constexpr float MinimumNormalLength = 1.0e-6f;
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= MinimumNormalLength || !std::isfinite(length))
        return {};

    const float inverseLength = 1.0f / length;
    FrustumPlane plane;
    plane.normal = cy::Vec3f(
        x * inverseLength, y * inverseLength, z * inverseLength);
    plane.distance = distance * inverseLength;
    plane.valid = std::isfinite(plane.distance);
    return plane;
}
}

Frustum Frustum::FromViewProjection(const cy::Matrix4f& matrix)
{
    Frustum frustum;

    // cy::Matrix4f stores columns contiguously. OpenGL clip inequalities are
    // extracted as row3 +/- row0, row1, and row2.
    frustum.m_Planes[Left] = MakePlane(
        matrix.cell[3] + matrix.cell[0],
        matrix.cell[7] + matrix.cell[4],
        matrix.cell[11] + matrix.cell[8],
        matrix.cell[15] + matrix.cell[12]);
    frustum.m_Planes[Right] = MakePlane(
        matrix.cell[3] - matrix.cell[0],
        matrix.cell[7] - matrix.cell[4],
        matrix.cell[11] - matrix.cell[8],
        matrix.cell[15] - matrix.cell[12]);
    frustum.m_Planes[Bottom] = MakePlane(
        matrix.cell[3] + matrix.cell[1],
        matrix.cell[7] + matrix.cell[5],
        matrix.cell[11] + matrix.cell[9],
        matrix.cell[15] + matrix.cell[13]);
    frustum.m_Planes[Top] = MakePlane(
        matrix.cell[3] - matrix.cell[1],
        matrix.cell[7] - matrix.cell[5],
        matrix.cell[11] - matrix.cell[9],
        matrix.cell[15] - matrix.cell[13]);
    frustum.m_Planes[Near] = MakePlane(
        matrix.cell[3] + matrix.cell[2],
        matrix.cell[7] + matrix.cell[6],
        matrix.cell[11] + matrix.cell[10],
        matrix.cell[15] + matrix.cell[14]);
    frustum.m_Planes[Far] = MakePlane(
        matrix.cell[3] - matrix.cell[2],
        matrix.cell[7] - matrix.cell[6],
        matrix.cell[11] - matrix.cell[10],
        matrix.cell[15] - matrix.cell[14]);

    return frustum;
}

bool Frustum::IntersectsSphere(const cy::Vec3f& center, float radius) const
{
    if (radius <= 0.0f || !std::isfinite(radius))
        return true;

    for (const FrustumPlane& plane : m_Planes)
    {
        if (!plane.valid)
            continue;

        const float signedDistance =
            plane.normal.x * center.x +
            plane.normal.y * center.y +
            plane.normal.z * center.z + plane.distance;
        if (signedDistance < -radius)
            return false;
    }
    return true;
}
