// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <vector>
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"
#include "cyMatrix.h"

constexpr unsigned MaxShadowLights = 4;
constexpr unsigned MaxShadowCascades = 4;
struct ShadowLightFrame
{
    LightId lightId = InvalidLightId;
    LightType type = LightType::Directional;
    cy::Vec3f position{0.0f};
    float nearPlane = 0.01f, farPlane = 30.0f;
    float bias = 0.5f, normalBias = 2.0f;
    unsigned cascadeCount = 1;
    int baseLayer = -1, reflectionBaseLayer = -1, cubeIndex = -1;
    std::array<cy::Matrix4f, MaxShadowCascades> matrices{};
    std::array<cy::Matrix4f, MaxShadowCascades> reflectionMatrices{};
    std::array<float, MaxShadowCascades> splits{};
    std::array<float, MaxShadowCascades> worldTexelSizes{};
    std::array<float, MaxShadowCascades> depthRanges{};
    std::array<float, MaxShadowCascades> reflectionWorldTexelSizes{};
    std::array<float, MaxShadowCascades> reflectionDepthRanges{};
    float cascadeNear = 0.1f;
    bool legacy = false;
};
struct ShadowViewFrame
{
    cy::Matrix4f viewProjection = cy::Matrix4f::Identity();
    unsigned lightIndex = 0;
    unsigned layer = 0; // Array layer or 6*cubeIndex+face for point lights.
    bool point = false;
};
struct ShadowFrame
{
    std::vector<ShadowLightFrame> lights;
    std::vector<ShadowViewFrame> views;
    unsigned layers2D = 0, cubeCount = 0;
    unsigned resolution = 1024, pointResolution = 512;
    unsigned eligibleLights = 0;
};
