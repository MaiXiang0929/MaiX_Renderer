// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/View/ShadowFrameBuilder.h"
#include "Engine/Rendering/Renderer/View/DirectionalShadow.h"
#include "Engine/Rendering/Renderer/View/SpotShadow.h"
#include "Engine/Rendering/Renderer/View/LightRenderData.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
bool Finite(const cy::Vec3f& v)
{ return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
cy::Vec3f Point(const cy::Matrix4f& matrix, const cy::Vec3f& p)
{
    const auto h = matrix * cy::Vec4f(p.x, p.y, p.z, 1);
    return cy::Vec3f(h.x, h.y, h.z) / h.w;
}
bool MatrixFinite(const cy::Matrix4f& matrix)
{
    for (float v : matrix.cell) if (!std::isfinite(v)) return false;
    return true;
}
// Sphere bounds keep scale stable under camera rotation. XY coordinates are snapped
// in a fixed world-origin light basis; snapping a camera-relative center would drift.
bool CascadeMatrix(const LightSceneProxy& light, const PrimitiveBounds& casters,
    const cy::Matrix4f& projection, const cy::Matrix4f& view, float start, float end,
    unsigned resolution, float padding, cy::Matrix4f& result, float& texel, float& depthRange)
{
    const auto inverseProjection = projection.GetInverse();
    const auto inverseView = view.GetInverse();
    std::array<cy::Vec3f, 8> corners;
    cy::Vec3f center(0.0f);
    unsigned index = 0;
    for (float y : {-1.0f, 1.0f}) for (float x : {-1.0f, 1.0f})
    {
        const auto a = Point(inverseProjection, {x, y, -1});
        const auto b = Point(inverseProjection, {x, y, 1});
        if (!Finite(a) || !Finite(b) || std::abs(b.z - a.z) < 1e-8f) return false;
        for (float distance : {start, end})
        {
            const auto p = a + (b - a) * ((-distance - a.z) / (b.z - a.z));
            corners[index] = p;
            center += p;
            ++index;
        }
    }
    center /= 8.0f;
    float radius = 0;
    for (const auto& p : corners) radius = std::max(radius, (p - center).Length());
    // Round outward; extra texel covers center snapping at both edges.
    radius = std::ceil((radius + padding) * 16) / 16;
    radius = std::max(radius, 0.001f) * resolution / (resolution - 2.0f);
    texel = 2 * radius / resolution;
    auto direction = light.direction;
    direction.Normalize();
    const auto up = std::abs(direction.y) > 0.99f ? cy::Vec3f(0,0,1) : cy::Vec3f(0,1,0);
    const auto basis = cy::Matrix4f::View(cy::Vec3f(0.0f), direction, up);
    const auto lightCenter = Point(basis, Point(inverseView, center));
    const float centerX = std::floor(lightCenter.x / texel + 0.5f) * texel;
    const float centerY = std::floor(lightCenter.y / texel + 0.5f) * texel;
    float minimumZ = std::numeric_limits<float>::max(), maximumZ = -minimumZ;
    for (const auto& p : corners)
    {
        const float z = Point(basis, Point(inverseView, p)).z;
        minimumZ = std::min(minimumZ, z - padding);
        maximumZ = std::max(maximumZ, z + padding);
    }
    // Include off-camera casters: clipping to the receiver frustum loses long shadows.
    const float casterZ = Point(basis, casters.center).z;
    minimumZ = std::min(minimumZ, casterZ - casters.radius - padding);
    maximumZ = std::max(maximumZ, casterZ + casters.radius + padding);
    depthRange = std::max(maximumZ - minimumZ, 0.001f);
    auto ortho = cy::Matrix4f::Identity();
    ortho.cell[0] = ortho.cell[5] = 1 / radius;
    ortho.cell[10] = -2 / depthRange;
    ortho.cell[12] = -centerX / radius; ortho.cell[13] = -centerY / radius;
    ortho.cell[14] = (maximumZ + minimumZ) / depthRange;
    result = ortho * basis;
    return MatrixFinite(result);
}
}

ShadowFrame BuildShadowFrame(const std::vector<LightSceneProxy>& lights, LightId mainLight,
    const ShadowSettings& settings, const PrimitiveBounds& casterBounds,
    const cy::Matrix4f& projection, const cy::Matrix4f& view,
    const cy::Matrix4f& reflectionView, bool reflectionEnabled, float displacementPadding)
{
    ShadowFrame frame;
    frame.resolution = settings.resolution; frame.pointResolution = settings.pointResolution;
    if (!IsValidShadowSettings(settings) || !Finite(casterBounds.center) ||
        !std::isfinite(casterBounds.radius) || casterBounds.radius <= 0 ||
        !std::isfinite(displacementPadding) || displacementPadding < 0) return frame;
    std::vector<const LightSceneProxy*> eligible;
    unsigned forwarded = 0;
    for (const auto& light : lights)
    {
        if (!light.visible) continue;
        if (forwarded++ >= MaxForwardLights) break;
        if (!light.castsShadow || !SupportsShadowMap(light.type) || !IsValidLightShadowSettings(light) ||
            !Finite(light.position) || !Finite(light.direction) ||
            (light.type != LightType::Point && light.direction.LengthSquared() < 1e-12f) ||
            (light.type != LightType::Directional && (!std::isfinite(light.range) || light.range <= 0))) continue;
        eligible.push_back(&light);
    }
    frame.eligibleLights = static_cast<unsigned>(eligible.size());
    const auto main = std::find_if(eligible.begin(), eligible.end(), [=](const auto* l){ return l->id == mainLight; });
    if (main != eligible.end()) std::rotate(eligible.begin(), main, main + 1);
    if (!settings.multiLight)
    {
        if (eligible.empty() || eligible.front()->id != mainLight) return frame;
        eligible.resize(1);
    }
    else if (eligible.size() > settings.maxLights) eligible.resize(settings.maxLights);
    for (const auto* source : eligible)
    {
        const auto& light = *source;
        ShadowLightFrame data;
        data.lightId = light.id; data.type = light.type; data.position = light.position;
        data.bias = light.shadowBias; data.normalBias = light.shadowNormalBias;
        data.legacy = !settings.multiLight;
        data.farPlane = light.range; data.nearPlane = light.range * 0.001f;
        const unsigned lightIndex = static_cast<unsigned>(frame.lights.size());
        std::vector<ShadowViewFrame> views;
        if (light.type == LightType::Point)
        {
            data.cubeIndex = static_cast<int>(frame.cubeCount);
            const auto perspective = cy::Matrix4f::Perspective(1.5707963267948966f, 1, data.nearPlane, data.farPlane);
            const cy::Vec3f directions[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            const cy::Vec3f ups[] = {{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
            for (unsigned face = 0; face < 6; ++face)
                views.push_back({perspective * cy::Matrix4f::View(cy::Vec3f(0.0f),directions[face],ups[face]) *
                    cy::Matrix4f::Translation(-light.position), lightIndex, frame.cubeCount * 6 + face, true});
        }
        else if (light.type == LightType::Spot)
        {
            cy::Vec2f bias;
            if (!BuildSpotShadow(light, data.matrices[0], bias)) continue;
            data.baseLayer = static_cast<int>(frame.layers2D);
            data.worldTexelSizes[0] = 2 * light.range * std::tan(light.outerConeAngle) / settings.resolution;
            data.depthRanges[0] = data.farPlane - data.nearPlane;
            views.push_back({data.matrices[0], lightIndex, frame.layers2D, false});
        }
        else
        {
            data.cascadeCount = settings.multiLight ? light.cascadeCount : 1;
            data.baseLayer = static_cast<int>(frame.layers2D);
            data.nearPlane = data.cascadeNear;
            data.farPlane = light.shadowDistance;
            bool valid = true;
            float start = data.cascadeNear;
            for (unsigned cascade = 0; cascade < data.cascadeCount; ++cascade)
            {
                const float fraction = static_cast<float>(cascade + 1) / data.cascadeCount;
                const float logarithmic = data.cascadeNear * std::pow(light.shadowDistance / data.cascadeNear, fraction);
                const float uniform = data.cascadeNear + (light.shadowDistance - data.cascadeNear) * fraction;
                const float end = data.splits[cascade] = light.splitLambda * logarithmic + (1 - light.splitLambda) * uniform;
                if (data.cascadeCount == 1)
                {
                    data.matrices[cascade] = FitDirectionalShadow(casterBounds, light.direction, displacementPadding);
                    const float radius = std::max(casterBounds.radius + displacementPadding, 0.001f) * 1.05f;
                    data.worldTexelSizes[cascade] = 2 * radius / settings.resolution;
                    data.depthRanges[cascade] = 2 * radius;
                }
                else valid &= CascadeMatrix(light,casterBounds,projection,view,
                    cascade == 0 ? start : start - (start - (cascade > 1 ? data.splits[cascade-2] : data.cascadeNear)) * 0.1f,
                    end,settings.resolution,
                    displacementPadding,data.matrices[cascade],data.worldTexelSizes[cascade],data.depthRanges[cascade]);
                views.push_back({data.matrices[cascade], lightIndex, frame.layers2D + cascade, false});
                start = end;
            }
            if (reflectionEnabled && data.cascadeCount > 1)
            {
                data.reflectionBaseLayer = static_cast<int>(frame.layers2D + data.cascadeCount);
                start = data.cascadeNear;
                for (unsigned cascade = 0; cascade < data.cascadeCount; ++cascade)
                {
                    valid &= CascadeMatrix(light,casterBounds,projection,reflectionView,
                        cascade == 0 ? start : start - (start - (cascade > 1 ? data.splits[cascade-2] : data.cascadeNear)) * 0.1f,
                        data.splits[cascade],settings.resolution,
                        displacementPadding,data.reflectionMatrices[cascade],data.reflectionWorldTexelSizes[cascade],data.reflectionDepthRanges[cascade]);
                    views.push_back({data.reflectionMatrices[cascade],lightIndex,static_cast<unsigned>(data.reflectionBaseLayer) + cascade,false});
                    start = data.splits[cascade];
                }
            }
            if (!valid) continue;
        }
        if (std::any_of(views.begin(),views.end(),[](const auto& v){ return !MatrixFinite(v.viewProjection); })) continue;
        if (light.type == LightType::Point) ++frame.cubeCount;
        else frame.layers2D += static_cast<unsigned>(views.size());
        frame.lights.push_back(data);
        frame.views.insert(frame.views.end(),views.begin(),views.end());
    }
    return frame;
}
