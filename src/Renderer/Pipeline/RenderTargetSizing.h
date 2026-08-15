// SPDX-License-Identifier: MIT
#pragma once

struct RenderTargetExtent
{
    unsigned int width = 0;
    unsigned int height = 0;
};

/// Reflection keeps the main-view aspect ratio at half resolution.
constexpr RenderTargetExtent CalculateReflectionTargetExtent(
    unsigned int viewportWidth,
    unsigned int viewportHeight)
{
    if (viewportWidth == 0 || viewportHeight == 0)
        return {};

    return {
        viewportWidth / 2 + viewportWidth % 2,
        viewportHeight / 2 + viewportHeight % 2
    };
}

/// Bloom uses a half-resolution HDR working set to limit blur bandwidth.
constexpr RenderTargetExtent CalculateBloomTargetExtent(
    unsigned int viewportWidth,
    unsigned int viewportHeight)
{
    if (viewportWidth == 0 || viewportHeight == 0)
        return {};

    return {
        viewportWidth / 2 + viewportWidth % 2,
        viewportHeight / 2 + viewportHeight % 2
    };
}

/// Editor overlays match the display target so pixel-sized gizmos stay stable.
constexpr RenderTargetExtent CalculateEditorOverlayTargetExtent(
    unsigned int viewportWidth,
    unsigned int viewportHeight)
{
    if (viewportWidth == 0 || viewportHeight == 0)
        return {};
    return { viewportWidth, viewportHeight };
}
