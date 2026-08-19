// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "cyMatrix.h"
#include "Renderer/Scene/LightSceneProxy.h"

constexpr std::size_t MaxForwardLights = 16;

struct alignas(16) GpuLightData
{
    std::array<float, 4> positionAndType{};
    std::array<float, 4> directionAndRange{};
    std::array<float, 4> colorAndIntensity{};
    std::array<float, 4> spotAnglesAndShadow{};
};

static_assert(sizeof(GpuLightData) == 64,
    "GpuLightData must match four std140 vec4 values.");
static_assert(offsetof(GpuLightData, positionAndType) == 0);
static_assert(offsetof(GpuLightData, directionAndRange) == 16);
static_assert(offsetof(GpuLightData, colorAndIntensity) == 32);
static_assert(offsetof(GpuLightData, spotAnglesAndShadow) == 48);

struct LightUploadData
{
    std::array<GpuLightData, MaxForwardLights> lights{};
    std::size_t sourceLightCount = 0;
    std::size_t lightCount = 0;
    int shadowLightIndex = -1;
    int keyLightIndex = -1;
    bool truncated = false;
};

LightUploadData BuildLightUploadData(
    const std::vector<LightSceneProxy>& lights,
    const cy::Matrix4f& view,
    LightId shadowLightId,
    LightId keyLightId);
