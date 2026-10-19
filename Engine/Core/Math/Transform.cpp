// SPDX-License-Identifier: MIT
#include "Engine/Core/Math/Transform.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
}

cy::Matrix4f Transform::ToMatrix() const
{
    return cy::Matrix4f::Translation(position) *
           cy::Matrix4f::RotationZ(rotationDegrees.z * DegreesToRadians) *
           cy::Matrix4f::RotationY(rotationDegrees.y * DegreesToRadians) *
           cy::Matrix4f::RotationX(rotationDegrees.x * DegreesToRadians) * cy::Matrix4f::Scale(scale);
}

bool TryDecomposeTransform(const cy::Matrix4f &m, Transform &result)
{
    for (float value : m.cell)
        if (!std::isfinite(value))
            return false;
    if (std::abs(m.cell[3]) + std::abs(m.cell[7]) + std::abs(m.cell[11]) + std::abs(m.cell[15] - 1) > 1e-5f)
        return false;
    cy::Vec3f x(m.cell[0], m.cell[1], m.cell[2]), y(m.cell[4], m.cell[5], m.cell[6]),
        z(m.cell[8], m.cell[9], m.cell[10]);
    Transform candidate;
    candidate.position = {m.cell[12], m.cell[13], m.cell[14]};
    candidate.scale = {x.Length(), y.Length(), z.Length()};
    if (!std::isfinite(candidate.scale.x) || !std::isfinite(candidate.scale.y) ||
        !std::isfinite(candidate.scale.z))
        return false;
    if (candidate.scale.x < 1e-5f || candidate.scale.y < 1e-5f || candidate.scale.z < 1e-5f)
        return false;
    x /= candidate.scale.x;
    y /= candidate.scale.y;
    z /= candidate.scale.z;
    if (std::abs(x.Dot(y)) > 1e-4f || std::abs(x.Dot(z)) > 1e-4f || std::abs(y.Dot(z)) > 1e-4f)
        return false;
    if (x.Cross(y).Dot(z) < 0)
    {
        x = -x;
        candidate.scale.x = -candidate.scale.x;
    }
    const float ry = std::asin(std::clamp(-x.z, -1.0f, 1.0f));
    if (std::abs(std::cos(ry)) > 1e-4f)
        candidate.rotationDegrees = {std::atan2(y.z, z.z) / DegreesToRadians, ry / DegreesToRadians,
                                     std::atan2(x.y, x.x) / DegreesToRadians};
    else
        candidate.rotationDegrees = {0, ry / DegreesToRadians, std::atan2(-y.x, y.y) / DegreesToRadians};
    const auto check = candidate.ToMatrix();
    for (int i = 0; i < 16; ++i)
        if (std::abs(check.cell[i] - m.cell[i]) > 0.001f * std::max(1.0f, std::abs(m.cell[i])))
            return false;
    result = candidate;
    return true;
}
