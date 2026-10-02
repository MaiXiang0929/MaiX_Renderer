// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "Core/Camera.h"
#include "Core/CameraClipRange.h"

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
    std::cerr << "[CameraTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

void TestTargetAffectsViewAndPosition()
{
    Camera camera(cy::Vec3f(3.0f, 4.0f, 5.0f), 10.0f);
    const cy::Vec3f position = camera.GetPosition();
    Require(NearlyEqual(position.x, 3.0f) &&
            NearlyEqual(position.y, 4.0f) &&
            NearlyEqual(position.z, 15.0f),
        "Camera position should be relative to its target.");

    const cy::Vec4f viewPosition = camera.GetViewMatrix() *
        cy::Vec4f(position.x, position.y, position.z, 1.0f);
    Require(NearlyEqual(viewPosition.x, 0.0f) &&
            NearlyEqual(viewPosition.y, 0.0f) &&
            NearlyEqual(viewPosition.z, 0.0f),
        "View matrix should transform the camera position to the origin.");
}

void TestPanAndDolly()
{
    Camera camera(cy::Vec3f(0.0f), 10.0f);
    camera.ProcessMousePan(100.0f, 0.0f, 1000.0f);
    Require(camera.GetTarget().x < 0.0f,
        "Dragging right should pan the viewed content to the right.");

    const float initialDistance = camera.GetDistance();
    camera.ProcessMouseZoom(-10.0f);
    Require(camera.GetDistance() < initialDistance,
        "Negative dolly input should move toward the target.");
}

void TestFocusAndPitchClamp()
{
    Camera camera;
    camera.SetAspectRatio(16.0f / 9.0f);
    camera.FocusBounds(cy::Vec3f(2.0f, 3.0f, 4.0f), 5.0f);
    Require(camera.GetTarget() == cy::Vec3f(2.0f, 3.0f, 4.0f) &&
            camera.GetDistance() > 5.0f,
        "Focus should target and frame the supplied bounds.");
    camera.ProcessMouseOrbit(0.0f, 100000.0f);
    const cy::Vec3f position = camera.GetPosition();
    Require(std::isfinite(position.x) && std::isfinite(position.y) &&
            std::isfinite(position.z),
        "Pitch clamping should keep the camera basis finite.");
}

void TestSmallBoundsAcrossViewportShapes()
{
    for (float aspect : {0.4f, 1.0f, 2.0f})
    {
        Camera camera;
        camera.SetAspectRatio(aspect);
        const cy::Vec3f center(0.1f, 0.2f, 0.3f);
        constexpr float radius = 0.005f;
        camera.FocusBounds(center, radius);
        const cy::Matrix4f vp = camera.GetProjectionMatrix() * camera.GetViewMatrix();
        // Sample the sphere, including its front/back, rather than only its center.
        for (int latitude = 0; latitude <= 12; ++latitude)
            for (int longitude = 0; longitude < 24; ++longitude)
            {
                const float theta = latitude * 3.14159265f / 12.0f;
                const float phi = longitude * 6.2831853f / 24.0f;
                const cy::Vec3f point = center + radius * cy::Vec3f(
                    std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
                const cy::Vec4f clip = vp * cy::Vec4f(point.x, point.y, point.z, 1.0f);
                Require(clip.w > 0.0f && std::abs(clip.x / clip.w) <= 1.0f &&
                    std::abs(clip.y / clip.w) <= 1.0f && std::abs(clip.z / clip.w) <= 1.0f,
                    "Small focused bounds must fit both viewport axes and depth planes.");
            }
        const float distance = camera.GetDistance();
        camera.FocusBounds(center, std::numeric_limits<float>::quiet_NaN());
        Require(camera.GetDistance() == distance, "Invalid bounds must not change the camera.");
    }
}

void TestSharedClipCoverage()
{
    CameraClipRange range;
    Require(range.Near() == 0.1f && range.Far() == 1000.0f,
        "Empty depth coverage should use valid fallback planes.");
    const cy::Matrix4f view = cy::Matrix4f::Identity();
    range.IncludeSphere(view, cy::Vec3f(0.0f, 0.0f, 2.0f), 0.1f);
    Require(range.Far() == 1000.0f, "Bounds entirely behind the camera should not affect planes.");
    range.IncludeSphere(view, cy::Vec3f(0.0f, 0.0f, -0.02f), 0.005f);
    Require(range.Near() < 0.015f, "Centimeter-sized objects must not be cut by the old near plane.");
    range.IncludeSphere(view, cy::Vec3f(0.0f, 0.0f, -1500.0f), 20.0f);
    Require(range.Far() > 1520.0f, "Ground/cone coverage must be allowed beyond the old far plane.");
    const cy::Matrix4f reflectionView = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, -2000.0f));
    range.IncludeSphere(reflectionView, cy::Vec3f(0.0f), 5.0f);
    Require(range.Far() > 2005.0f, "Shared projection must cover reflection depth as well.");
    range.IncludeSphere(view, cy::Vec3f(0.0f), 0.1f);
    Require(range.Near() == 0.0001f && range.Far() > range.Near(),
        "Intersecting bounds should use a positive minimum near plane.");
}
}

int main()
{
    TestTargetAffectsViewAndPosition();
    TestPanAndDolly();
    TestFocusAndPitchClamp();
    TestSmallBoundsAcrossViewportShapes();
    TestSharedClipCoverage();
    std::cout << "Camera tests passed." << std::endl;
    return EXIT_SUCCESS;
}
