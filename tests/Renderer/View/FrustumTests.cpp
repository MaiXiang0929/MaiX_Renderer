// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "Engine/Rendering/Renderer/Scene/PrimitiveSceneProxy.h"
#include "Engine/Rendering/Renderer/View/Frustum.h"

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

    std::cerr << "[FrustumTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

void TestIdentityFrustum()
{
    const Frustum frustum = Frustum::FromViewProjection(
        cy::Matrix4f::Identity());

    Require(frustum.IntersectsSphere(cy::Vec3f(0.0f, 0.0f, 0.0f), 0.25f),
        "Sphere at the origin should be visible.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(-2.0f, 0.0f, 0.0f), 0.25f),
        "Sphere outside the left plane should be culled.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(2.0f, 0.0f, 0.0f), 0.25f),
        "Sphere outside the right plane should be culled.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(0.0f, -2.0f, 0.0f), 0.25f),
        "Sphere outside the bottom plane should be culled.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(0.0f, 2.0f, 0.0f), 0.25f),
        "Sphere outside the top plane should be culled.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(0.0f, 0.0f, -2.0f), 0.25f),
        "Sphere outside the near plane should be culled.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(0.0f, 0.0f, 2.0f), 0.25f),
        "Sphere outside the far plane should be culled.");
    Require(frustum.IntersectsSphere(cy::Vec3f(1.1f, 0.0f, 0.0f), 0.2f),
        "Sphere intersecting a frustum plane should be retained.");
    Require(frustum.IntersectsSphere(cy::Vec3f(100.0f, 0.0f, 0.0f), 0.0f),
        "Invalid bounds should use conservative visibility.");
}

void TestPerspectiveFrustum()
{
    cy::Matrix4f projection;
    projection.SetPerspective(
        60.0f * 3.14159265358979323846f / 180.0f,
        1.0f, 0.1f, 100.0f);
    const Frustum frustum = Frustum::FromViewProjection(projection);

    Require(frustum.IntersectsSphere(cy::Vec3f(0.0f, 0.0f, -5.0f), 0.5f),
        "Sphere inside perspective frustum should be visible.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(10.0f, 0.0f, -5.0f), 0.5f),
        "Sphere outside perspective side plane should be culled.");
    Require(!frustum.IntersectsSphere(cy::Vec3f(0.0f, 0.0f, 1.0f), 0.1f),
        "Sphere behind perspective camera should be culled.");
}

void TestTransformedBounds()
{
    PrimitiveBounds localBounds;
    localBounds.center = cy::Vec3f(1.0f, 2.0f, 3.0f);
    localBounds.radius = 2.0f;

    const cy::Matrix4f transform =
        cy::Matrix4f::Translation(cy::Vec3f(4.0f, 5.0f, 6.0f)) *
        cy::Matrix4f::Scale(2.0f, 3.0f, 4.0f);
    const PrimitiveBounds worldBounds = TransformBounds(localBounds, transform);

    Require(NearlyEqual(worldBounds.center.x, 6.0f) &&
        NearlyEqual(worldBounds.center.y, 11.0f) &&
        NearlyEqual(worldBounds.center.z, 18.0f),
        "Bounds center should use the complete local-to-world transform.");
    Require(NearlyEqual(worldBounds.radius, 8.0f),
        "Bounds radius should use the largest transform axis scale.");
}
}

int main()
{
    TestIdentityFrustum();
    TestPerspectiveFrustum();
    TestTransformedBounds();
    std::cout << "Frustum tests passed." << std::endl;
    return EXIT_SUCCESS;
}
