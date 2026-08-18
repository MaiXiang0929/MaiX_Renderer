// SPDX-License-Identifier: MIT
#include "Transform.h"

namespace
{
constexpr float DegreesToRadians =
    3.14159265358979323846f / 180.0f;
}

cy::Matrix4f Transform::ToMatrix() const
{
    return cy::Matrix4f::Translation(position) *
        cy::Matrix4f::RotationZ(rotationDegrees.z * DegreesToRadians) *
        cy::Matrix4f::RotationY(rotationDegrees.y * DegreesToRadians) *
        cy::Matrix4f::RotationX(rotationDegrees.x * DegreesToRadians) *
        cy::Matrix4f::Scale(scale);
}
