// SPDX-License-Identifier: MIT
#include "ViewportPicking.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"

namespace
{
cy::Vec3f HomogeneousPoint(const cy::Vec4f& value)
{
    if (std::abs(value.w) <= 1.0e-8f)
        return cy::Vec3f(value.x, value.y, value.z);
    return cy::Vec3f(value.x / value.w, value.y / value.w, value.z / value.w);
}
}

WorldRay BuildWorldRay(
    float framebufferX,
    float framebufferY,
    float framebufferWidth,
    float framebufferHeight,
    const cy::Matrix4f& projection,
    const cy::Matrix4f& view)
{
    WorldRay ray;
    if (framebufferWidth <= 0.0f || framebufferHeight <= 0.0f)
        return ray;

    const float ndcX = 2.0f * framebufferX / framebufferWidth - 1.0f;
    const float ndcY = 1.0f - 2.0f * framebufferY / framebufferHeight;
    const cy::Matrix4f inverseViewProjection =
        (projection * view).GetInverse();
    const cy::Vec3f nearPoint = HomogeneousPoint(
        inverseViewProjection * cy::Vec4f(ndcX, ndcY, -1.0f, 1.0f));
    const cy::Vec3f farPoint = HomogeneousPoint(
        inverseViewProjection * cy::Vec4f(ndcX, ndcY, 1.0f, 1.0f));

    ray.origin = nearPoint;
    ray.direction = farPoint - nearPoint;
    if (ray.direction.Length() > 1.0e-8f)
        ray.direction.Normalize();
    return ray;
}

bool IntersectRaySphere(
    const WorldRay& ray,
    const cy::Vec3f& center,
    float radius,
    float& distance)
{
    if (radius <= 0.0f)
        return false;

    const cy::Vec3f offset = ray.origin - center;
    const float halfB = offset.Dot(ray.direction);
    const float c = offset.Dot(offset) - radius * radius;
    const float discriminant = halfB * halfB - c;
    if (discriminant < 0.0f)
        return false;

    const float root = std::sqrt(discriminant);
    const float nearDistance = -halfB - root;
    const float farDistance = -halfB + root;
    distance = nearDistance >= 0.0f ? nearDistance : farDistance;
    return distance >= 0.0f;
}

bool HitTestEditableModel(
    const WorldRay& ray,
    const EditableModel& model,
    float& distance)
{
    const cy::Matrix4f root = model.transform.ToMatrix();
    bool hit = false;
    distance = std::numeric_limits<float>::max();
    for (const EditableModelSection& section : model.sections)
    {
        const PrimitiveBounds bounds = TransformBounds(
            section.localBounds, root * section.localTransform);
        float sectionDistance = 0.0f;
        if (IntersectRaySphere(
                ray, bounds.center, bounds.radius, sectionDistance) &&
            sectionDistance < distance)
        {
            distance = sectionDistance;
            hit = true;
        }
    }
    return hit;
}

bool HitTestLightIcon(
    float framebufferX,
    float framebufferY,
    float framebufferWidth,
    float framebufferHeight,
    const cy::Matrix4f& viewProjection,
    const cy::Vec3f& worldPosition,
    float radiusPixels,
    float& depth)
{
    if (framebufferWidth <= 0.0f || framebufferHeight <= 0.0f ||
        radiusPixels <= 0.0f)
        return false;

    const cy::Vec4f clip = viewProjection *
        cy::Vec4f(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
    if (clip.w <= 1.0e-6f)
        return false;

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float ndcZ = clip.z / clip.w;
    if (ndcZ < -1.0f || ndcZ > 1.0f)
        return false;

    const float screenX = (ndcX * 0.5f + 0.5f) * framebufferWidth;
    const float screenY = (0.5f - ndcY * 0.5f) * framebufferHeight;
    const float deltaX = framebufferX - screenX;
    const float deltaY = framebufferY - screenY;
    depth = ndcZ;
    return deltaX * deltaX + deltaY * deltaY <= radiusPixels * radiusPixels;
}

EditorPickResult PickEditorObject(
    float framebufferX,
    float framebufferY,
    float framebufferWidth,
    float framebufferHeight,
    const cy::Matrix4f& projection,
    const cy::Matrix4f& view,
    const EditableModel& model,
    const std::vector<EditableLight>& lights)
{
    EditorPickResult result;
    float nearestLightDepth = 1.0f;
    const cy::Matrix4f viewProjection = projection * view;
    for (const EditableLight& light : lights)
    {
        if (!light.IsValid() || light.proxy.type == LightType::Directional)
            continue;

        float depth = 0.0f;
        if (HitTestLightIcon(
                framebufferX,
                framebufferY,
                framebufferWidth,
                framebufferHeight,
                viewProjection,
                light.transform.position,
                18.0f,
                depth) &&
            (result.type != EditorSelectionType::Light ||
             depth < nearestLightDepth))
        {
            result.type = EditorSelectionType::Light;
            result.lightId = light.proxy.id;
            nearestLightDepth = depth;
        }
    }
    if (result.type == EditorSelectionType::Light)
        return result;

    const WorldRay ray = BuildWorldRay(
        framebufferX,
        framebufferY,
        framebufferWidth,
        framebufferHeight,
        projection,
        view);
    float modelDistance = 0.0f;
    if (HitTestEditableModel(ray, model, modelDistance))
        result.type = EditorSelectionType::Model;
    return result;
}
