// SPDX-License-Identifier: MIT
#include "EditableModel.h"

#include <algorithm>

PrimitiveBounds EditableModel::GetWorldBounds() const
{
    PrimitiveBounds result;
    if (sections.empty())
        return result;

    const cy::Matrix4f root = transform.ToMatrix();
    bool initialized = false;
    for (const EditableModelSection& section : sections)
    {
        const PrimitiveBounds bounds = TransformBounds(
            section.localBounds, root * section.localTransform);
        if (bounds.radius <= 0.0f)
            continue;

        if (!initialized)
        {
            result = bounds;
            initialized = true;
            continue;
        }

        const cy::Vec3f delta = bounds.center - result.center;
        const float distance = delta.Length();
        if (distance + bounds.radius <= result.radius)
            continue;
        if (distance + result.radius <= bounds.radius)
        {
            result = bounds;
            continue;
        }

        const float mergedRadius =
            (distance + result.radius + bounds.radius) * 0.5f;
        if (distance > 1.0e-6f)
            result.center += delta * ((mergedRadius - result.radius) / distance);
        result.radius = mergedRadius;
    }
    return result;
}
