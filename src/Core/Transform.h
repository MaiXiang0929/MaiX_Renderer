// SPDX-License-Identifier: MIT
#pragma once

#include "cyMatrix.h"
#include "cyVector.h"

struct Transform
{
    cy::Vec3f position{ 0.0f, 0.0f, 0.0f };
    cy::Vec3f rotationDegrees{ 0.0f, 0.0f, 0.0f };
    cy::Vec3f scale{ 1.0f, 1.0f, 1.0f };

    cy::Matrix4f ToMatrix() const;
};
