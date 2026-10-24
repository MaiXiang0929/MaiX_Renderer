// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/View/LightRenderData.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float MinimumRange = 1.0e-4f;
constexpr float MinimumDirectionLength = 1.0e-6f;

cy::Vec3f TransformPoint(const cy::Matrix4f& matrix, const cy::Vec3f& point)
{
    const cy::Vec4f transformed = matrix * cy::Vec4f(
        point.x, point.y, point.z, 1.0f);
    return cy::Vec3f(transformed.x, transformed.y, transformed.z);
}

cy::Vec3f TransformDirection(
    const cy::Matrix4f& matrix,
    const cy::Vec3f& direction)
{
    const cy::Vec4f transformed = matrix * cy::Vec4f(
        direction.x, direction.y, direction.z, 0.0f);
    cy::Vec3f result(transformed.x, transformed.y, transformed.z);
    if (result.Length() <= MinimumDirectionLength)
        result = cy::Vec3f(0.0f, -1.0f, 0.0f);
    else
        result.Normalize();
    return result;
}

float ClampAngle(float angle)
{
    return std::clamp(angle, 0.0f, Pi);
}
}

LightUploadData BuildLightUploadData(
    const std::vector<LightSceneProxy>& lights,
    const cy::Matrix4f& view,
    LightId shadowLightId,
    LightId keyLightId,
    const ShadowFrame* shadowFrame)
{
    LightUploadData upload;

    for (const LightSceneProxy& light : lights)
    {
        ++upload.sourceLightCount;
        if (upload.lightCount >= MaxForwardLights)
        {
            upload.truncated = true;
            continue;
        }

        GpuLightData& gpuLight = upload.lights[upload.lightCount];
        const cy::Vec3f positionView = TransformPoint(view, light.position);
        const cy::Vec3f directionView = TransformDirection(
            view, light.direction);
        const float innerAngle = ClampAngle(std::min(
            light.innerConeAngle, light.outerConeAngle));
        const float outerAngle = ClampAngle(std::max(
            light.innerConeAngle, light.outerConeAngle));

        gpuLight.positionAndType = {
            positionView.x,
            positionView.y,
            positionView.z,
            static_cast<float>(light.type)
        };
        gpuLight.directionAndRange = {
            directionView.x,
            directionView.y,
            directionView.z,
            std::max(light.range, MinimumRange)
        };
        gpuLight.colorAndIntensity = {
            std::max(light.color.x, 0.0f),
            std::max(light.color.y, 0.0f),
            std::max(light.color.z, 0.0f),
            std::max(light.intensity, 0.0f)
        };
        gpuLight.spotAnglesAndShadow = {
            std::cos(innerAngle),
            std::cos(outerAngle),
            light.castsShadow ? 1.0f : 0.0f,
            -1.0f
        };

        if (shadowFrame && light.castsShadow)
        {
            for (std::size_t index = 0;
                index < std::min(shadowFrame->lights.size(), std::size_t(MaxShadowLights));
                ++index)
            {
                if (shadowFrame->lights[index].lightId == light.id)
                {
                    gpuLight.spotAnglesAndShadow[3] = static_cast<float>(index);
                    break;
                }
            }
        }

        if (shadowLightId != InvalidLightId &&
            light.id == shadowLightId &&
            light.castsShadow &&
            SupportsShadowMap(light.type))
        {
            upload.shadowLightIndex = static_cast<int>(upload.lightCount);
        }
        if (keyLightId != InvalidLightId && light.id == keyLightId)
            upload.keyLightIndex = static_cast<int>(upload.lightCount);
        ++upload.lightCount;
    }

    return upload;
}
