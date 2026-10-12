// SPDX-License-Identifier: MIT
#pragma once
#include "cyMatrix.h"
#include "cyVector.h"

struct PrimitiveBounds
{
    cy::Vec3f center{ 0.0f, 0.0f, 0.0f };
    float radius = 0.0f;
};

PrimitiveBounds TransformBounds(
    const PrimitiveBounds& localBounds,
    const cy::Matrix4f& localToWorld);

