// SPDX-License-Identifier: MIT
#pragma once

#include <array>

#include "cyMatrix.h"
#include "cyVector.h"

struct FrustumPlane
{
    cy::Vec3f normal{ 0.0f, 0.0f, 0.0f };
    float distance = 0.0f;
    bool valid = false;
};

class Frustum
{
public:
    enum PlaneIndex
    {
        Left,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        PlaneCount
    };

    static Frustum FromViewProjection(const cy::Matrix4f& matrix);

    bool IntersectsSphere(const cy::Vec3f& center, float radius) const;

private:
    std::array<FrustumPlane, PlaneCount> m_Planes;
};
