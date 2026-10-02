// SPDX-License-Identifier: MIT
#pragma once
#include "Core/Transform.h"
#include <cstdint>
#include <string>
struct CameraId
{
    std::uint64_t value = 0;
    bool IsValid() const
    {
        return value != 0;
    }
    bool operator==(CameraId other) const
    {
        return value == other.value;
    }
};
// CPU scene record; Renderer consumes matrices and does not own a camera resource.
struct EditableCamera
{
    CameraId id;
    std::string name;
    Transform transform;
    float fovDegrees = 50, nearPlane = 0.1f, farPlane = 1000;
    cy::Matrix4f GetViewMatrix() const
    {
        Transform rigid = transform;
        rigid.scale = cy::Vec3f(1);
        return rigid.ToMatrix().GetInverse();
    }
    cy::Matrix4f GetProjectionMatrix(float aspect) const
    {
        return cy::Matrix4f::Perspective(fovDegrees * 3.14159265358979323846f / 180, aspect,
                                         nearPlane, farPlane);
    }
};
