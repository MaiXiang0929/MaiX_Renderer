// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>

#include "Renderer/Pipeline/RenderTargetSizing.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "[RenderTargetSizingTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}
}

int main()
{
    const RenderTargetExtent hd =
        CalculateReflectionTargetExtent(1920, 1080);
    Require(hd.width == 960 && hd.height == 540,
        "Even viewport dimensions should halve exactly.");

    const RenderTargetExtent odd =
        CalculateReflectionTargetExtent(1279, 719);
    Require(odd.width == 640 && odd.height == 360,
        "Odd viewport dimensions should round up independently.");

    const RenderTargetExtent minimum =
        CalculateReflectionTargetExtent(1, 1);
    Require(minimum.width == 1 && minimum.height == 1,
        "A valid viewport must produce a non-zero reflection target.");

    const RenderTargetExtent minimized =
        CalculateReflectionTargetExtent(0, 1080);
    Require(minimized.width == 0 && minimized.height == 0,
        "A minimized viewport should not request a render target.");

    const RenderTargetExtent bloomHd =
        CalculateBloomTargetExtent(1920, 1080);
    Require(bloomHd.width == 960 && bloomHd.height == 540,
        "Bloom should use an exact half-resolution target for even extents.");

    const RenderTargetExtent bloomOdd =
        CalculateBloomTargetExtent(1279, 719);
    Require(bloomOdd.width == 640 && bloomOdd.height == 360,
        "Bloom should round odd half-resolution extents up.");

    const RenderTargetExtent bloomMinimum =
        CalculateBloomTargetExtent(1, 1);
    Require(bloomMinimum.width == 1 && bloomMinimum.height == 1,
        "Bloom should preserve a valid one-pixel viewport.");

    const RenderTargetExtent overlayHd =
        CalculateEditorOverlayTargetExtent(1920, 1080);
    Require(overlayHd.width == 1920 && overlayHd.height == 1080,
        "Editor overlay should match the full display extent.");

    const RenderTargetExtent overlayMinimum =
        CalculateEditorOverlayTargetExtent(1, 1);
    Require(overlayMinimum.width == 1 && overlayMinimum.height == 1,
        "Editor overlay should preserve a valid one-pixel viewport.");

    const RenderTargetExtent overlayMinimized =
        CalculateEditorOverlayTargetExtent(1920, 0);
    Require(overlayMinimized.width == 0 && overlayMinimized.height == 0,
        "Editor overlay should reject a minimized viewport.");

    std::cout << "RenderTargetSizingTests passed." << std::endl;
    return EXIT_SUCCESS;
}
