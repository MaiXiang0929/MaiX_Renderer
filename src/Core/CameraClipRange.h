// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "cyMatrix.h"
#include "cyVector.h"

// CPU depth coverage for views sharing one projection. No scene/resource ownership.
class CameraClipRange
{
public:
    void IncludeSphere(const cy::Matrix4f& view, const cy::Vec3f& center, float radius)
    {
        if (!std::isfinite(radius) || radius < 0.0f)
            return;
        const cy::Vec4f position = view * cy::Vec4f(center.x, center.y, center.z, 1.0f);
        const float front = -position.z - radius;
        const float back = -position.z + radius;
        if (!std::isfinite(front) || !std::isfinite(back) || back <= 0.0f)
            return;
        m_Nearest = std::min(m_Nearest, front);
        m_Farthest = std::max(m_Farthest, back);
    }

    float Near() const
    {
        return HasCoverage() ? std::clamp(m_Nearest * 0.5f, 0.0001f, 0.1f) : 0.1f;
    }
    float Far() const
    {
        // Keep arithmetic finite for exceptionally large but finite input.
        const double far = static_cast<double>(m_Farthest) * 1.1 + 0.01;
        return HasCoverage() ? static_cast<float>(std::min(
            std::max(1.0, far), static_cast<double>(std::numeric_limits<float>::max()))) : 1000.0f;
    }

private:
    bool HasCoverage() const { return m_Farthest > 0.0f; }
    float m_Nearest = std::numeric_limits<float>::infinity();
    float m_Farthest = 0.0f;
};
