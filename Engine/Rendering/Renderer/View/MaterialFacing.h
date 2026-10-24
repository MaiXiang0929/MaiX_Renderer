// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>

#include "cyMatrix.h"
#include "Engine/Rendering/Material/MaterialTypes.h"
#include "Engine/Rendering/RHI/RHITypes.h"

inline bool IsSurfaceWindingReversed(SurfaceSide side, const cy::Matrix4f& modelView)
{
    if (!IsValidSurfaceSide(side))
        throw RHIError("Invalid material surface side");
    // Legacy deliberately retains the old CCW classification, including mirrors.
    if (side == SurfaceSide::Legacy)
        return false;
    const float determinant = modelView.GetSubMatrix3().GetDeterminant();
    if (!std::isfinite(determinant))
        throw RHIError("Non-finite surface model-view determinant");
    return determinant < 0.0f;
}

inline void ApplySurfaceFacing(RHIPipelineState& state, SurfaceSide side, bool reversedWinding)
{
    if (!IsValidSurfaceSide(side))
        throw RHIError("Invalid material surface side");
    state.cull = side == SurfaceSide::Front ? RHICull::Back :
        side == SurfaceSide::Back ? RHICull::Front : RHICull::None;
    state.frontCounterClockwise = side == SurfaceSide::Legacy || !reversedWinding;
}

inline bool SupportsSurfaceOutline(SurfaceSide side)
{
    if (!IsValidSurfaceSide(side))
        throw RHIError("Invalid material surface side");
    return side == SurfaceSide::Front || side == SurfaceSide::Legacy;
}
