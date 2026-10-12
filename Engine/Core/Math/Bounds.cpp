// SPDX-License-Identifier: MIT
#include "Engine/Core/Math/Bounds.h"

#include <algorithm>
#include <cmath>

PrimitiveBounds TransformBounds(
    const PrimitiveBounds& localBounds,
    const cy::Matrix4f& localToWorld)
{
    const cy::Vec4f transformedCenter = localToWorld * cy::Vec4f(
        localBounds.center.x,
        localBounds.center.y,
        localBounds.center.z,
        1.0f);

    const float scaleX = std::sqrt(
        localToWorld.cell[0] * localToWorld.cell[0] +
        localToWorld.cell[1] * localToWorld.cell[1] +
        localToWorld.cell[2] * localToWorld.cell[2]);
    const float scaleY = std::sqrt(
        localToWorld.cell[4] * localToWorld.cell[4] +
        localToWorld.cell[5] * localToWorld.cell[5] +
        localToWorld.cell[6] * localToWorld.cell[6]);
    const float scaleZ = std::sqrt(
        localToWorld.cell[8] * localToWorld.cell[8] +
        localToWorld.cell[9] * localToWorld.cell[9] +
        localToWorld.cell[10] * localToWorld.cell[10]);

    PrimitiveBounds worldBounds;
    worldBounds.center = cy::Vec3f(
        transformedCenter.x, transformedCenter.y, transformedCenter.z);
    worldBounds.radius = localBounds.radius *
        std::max({ scaleX, scaleY, scaleZ });
    return worldBounds;
}
