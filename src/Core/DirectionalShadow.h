// SPDX-License-Identifier: MIT
#pragma once
#include "Renderer/Scene/PrimitiveSceneProxy.h"
#include <algorithm>
#include <cmath>
// Conservative sphere fitting in light space. OpenGL camera forward is -Z.
inline cy::Matrix4f FitDirectionalShadow(const PrimitiveBounds &bounds, cy::Vec3f direction,
                                         float displacementPadding = 0)
{
    if (direction.LengthSquared() < 1e-12f)
        direction = {0, -1, 0};
    direction.Normalize();
    const float radius = std::max(bounds.radius + displacementPadding, 0.001f) * 1.05f;
    const float distance = radius * 2 + 0.01f;
    const auto up = std::abs(direction.y) > 0.99f ? cy::Vec3f(0, 0, 1) : cy::Vec3f(0, 1, 0);
    const auto view = cy::Matrix4f::View(bounds.center - direction * distance, bounds.center, up);
    const float nearPlane = distance - radius, farPlane = distance + radius;
    cy::Matrix4f projection;
    projection.SetIdentity();
    projection.cell[0] = 1 / radius;
    projection.cell[5] = 1 / radius;
    projection.cell[10] = -2 / (farPlane - nearPlane);
    projection.cell[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    return projection * view;
}
