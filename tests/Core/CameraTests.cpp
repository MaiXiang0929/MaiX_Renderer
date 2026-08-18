// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "Core/Camera.h"

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
}

int main()
{
    TestTargetAffectsViewAndPosition();
    TestPanAndDolly();
    TestFocusAndPitchClamp();
    std::cout << "Camera tests passed." << std::endl;
    return EXIT_SUCCESS;
}
