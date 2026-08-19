// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "Renderer/View/LightRenderData.h"

namespace
{
bool NearlyEqual(float left, float right, float epsilon = 1.0e-4f)
{
    return std::abs(left - right) <= epsilon;
}

void Require(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "[LightRenderDataTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

void TestViewSpacePacking()
{
    LightSceneProxy point;
    point.type = LightType::Point;
    point.position = cy::Vec3f(4.0f, 5.0f, 6.0f);
    point.direction = cy::Vec3f(0.0f, -2.0f, 0.0f);
    point.color = cy::Vec3f(0.25f, 0.5f, 1.0f);
    point.intensity = 3.0f;
    point.range = 12.0f;
    point.castsShadow = false;

    const std::vector<LightSceneProxy> lights{ point };
    const cy::Matrix4f view = cy::Matrix4f::Translation(
        cy::Vec3f(-1.0f, -2.0f, -3.0f));
    const LightUploadData upload = BuildLightUploadData(
        lights, view, InvalidLightId, InvalidLightId);
    const GpuLightData& gpu = upload.lights[0];

    Require(upload.sourceLightCount == 1 && upload.lightCount == 1,
        "One valid light should be uploaded.");
    Require(upload.shadowLightIndex == -1,
        "A non-shadow light must not be selected for the shadow map.");
    Require(upload.keyLightIndex == -1,
        "No key light should be selected without an explicit id.");
    Require(NearlyEqual(gpu.positionAndType[0], 3.0f) &&
        NearlyEqual(gpu.positionAndType[1], 3.0f) &&
        NearlyEqual(gpu.positionAndType[2], 3.0f),
        "Light position should be transformed into view space.");
    Require(NearlyEqual(gpu.positionAndType[3], 1.0f),
        "Point light type should be packed as 1.");
    Require(NearlyEqual(gpu.directionAndRange[0], 0.0f) &&
        NearlyEqual(gpu.directionAndRange[1], -1.0f) &&
        NearlyEqual(gpu.directionAndRange[2], 0.0f),
        "Direction should ignore translation and be normalized.");
    Require(NearlyEqual(gpu.directionAndRange[3], 12.0f),
        "Light range should be preserved.");
    Require(NearlyEqual(gpu.colorAndIntensity[3], 3.0f),
        "Light intensity should be preserved.");
}

void TestSpotAnglesAndShadowSelection()
{
    constexpr float Pi = 3.14159265358979323846f;
    LightSceneProxy fill;
    fill.id = 5;
    fill.castsShadow = false;

    LightSceneProxy spot;
    spot.id = 7;
    spot.type = LightType::Spot;
    spot.innerConeAngle = 30.0f * Pi / 180.0f;
    spot.outerConeAngle = 20.0f * Pi / 180.0f;
    spot.castsShadow = true;

    LightSceneProxy secondShadow;
    secondShadow.id = 9;
    secondShadow.castsShadow = true;

    const std::vector<LightSceneProxy> lights{ fill, spot, secondShadow };
    const LightUploadData upload = BuildLightUploadData(
        lights, cy::Matrix4f::Identity(), spot.id, fill.id);

    Require(upload.sourceLightCount == 3 && upload.lightCount == 3,
        "All light snapshots should be uploaded.");
    Require(upload.shadowLightIndex == 1,
        "The light matching shadowLightId should own the shadow map.");
    Require(upload.keyLightIndex == 0,
        "Key light selection must not require shadow casting.");
    Require(NearlyEqual(
        upload.lights[1].spotAnglesAndShadow[0],
        std::cos(20.0f * Pi / 180.0f)),
        "Spot inner angle should be the narrower cone.");
    Require(NearlyEqual(
        upload.lights[1].spotAnglesAndShadow[1],
        std::cos(30.0f * Pi / 180.0f)),
        "Spot outer angle should be the wider cone.");
    Require(NearlyEqual(upload.lights[1].spotAnglesAndShadow[2], 1.0f),
        "Shadow capability should be packed for diagnostics.");
    Require(NearlyEqual(upload.lights[1].positionAndType[3], 2.0f),
        "Spot light type should be packed as 2.");
}

void TestDirectionalPacking()
{
    LightSceneProxy directional;
    directional.type = LightType::Directional;
    directional.direction = cy::Vec3f(1.0f, -1.0f, 0.0f);
    directional.castsShadow = true;
    directional.id = 3;

    const std::vector<LightSceneProxy> lights{ directional };
    const cy::Matrix4f view = cy::Matrix4f::RotationZ(
        3.14159265358979323846f * 0.5f);
    const LightUploadData upload = BuildLightUploadData(
        lights, view, directional.id, directional.id);

    Require(NearlyEqual(upload.lights[0].positionAndType[3], 0.0f),
        "Directional light type should be packed as 0.");
    Require(NearlyEqual(upload.lights[0].directionAndRange[0], 0.7071067f) &&
        NearlyEqual(upload.lights[0].directionAndRange[1], 0.7071067f),
        "Directional light direction should be transformed into view space.");
    Require(upload.shadowLightIndex == 0,
        "Directional lights may reference a compatible 2D shadow map.");
    Require(upload.keyLightIndex == 0,
        "One light may be both the key light and shadow caster.");
}

void TestDeterministicTruncation()
{
    std::vector<LightSceneProxy> lights(MaxForwardLights + 3);
    for (std::size_t index = 0; index < lights.size(); ++index)
    {
        lights[index].id = static_cast<LightId>(index + 1);
        lights[index].position.x = static_cast<float>(index);
        lights[index].castsShadow = index == MaxForwardLights + 1;
    }

    const LightUploadData upload = BuildLightUploadData(
        lights,
        cy::Matrix4f::Identity(),
        lights[MaxForwardLights + 1].id,
        lights[MaxForwardLights + 1].id);

    Require(upload.sourceLightCount == MaxForwardLights + 3,
        "Source count should include lights beyond the GPU limit.");
    Require(upload.lightCount == MaxForwardLights && upload.truncated,
        "GPU light data should truncate at MaxForwardLights.");
    Require(upload.shadowLightIndex == -1,
        "A truncated shadow caster cannot own the uploaded shadow map.");
    Require(upload.keyLightIndex == -1,
        "A truncated key light cannot drive Face Shadow.");
    Require(NearlyEqual(
        upload.lights[MaxForwardLights - 1].positionAndType[0],
        static_cast<float>(MaxForwardLights - 1)),
        "Truncation should retain the first lights deterministically.");
}
}

int main()
{
    TestViewSpacePacking();
    TestSpotAnglesAndShadowSelection();
    TestDirectionalPacking();
    TestDeterministicTruncation();
    std::cout << "Light render data tests passed." << std::endl;
    return EXIT_SUCCESS;
}
