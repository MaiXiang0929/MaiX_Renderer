// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "Core/Camera.h"
#include "Core/Transform.h"
#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"
#include "Editor/EditorSelection.h"
#include "Editor/ViewportPicking.h"

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
    std::cerr << "[EditorInteractionTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

void TestTransformComposition()
{
    Transform transform;
    transform.position = cy::Vec3f(4.0f, 5.0f, 6.0f);
    transform.scale = cy::Vec3f(2.0f, 3.0f, 4.0f);
    const cy::Vec4f point = transform.ToMatrix() *
        cy::Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    Require(NearlyEqual(point.x, 6.0f) &&
            NearlyEqual(point.y, 8.0f) &&
            NearlyEqual(point.z, 10.0f),
        "Root transform should apply scale before translation.");
}

void TestCenterRayAndModelHit()
{
    Camera camera(cy::Vec3f(0.0f), 10.0f);
    camera.SetAspectRatio(1.0f);
    const WorldRay ray = BuildWorldRay(
        500.0f,
        500.0f,
        1000.0f,
        1000.0f,
        camera.GetProjectionMatrix(),
        camera.GetViewMatrix());
    Require(std::abs(ray.direction.x) < 1.0e-4f &&
            std::abs(ray.direction.y) < 1.0e-4f &&
            ray.direction.z < -0.999f,
        "Center-screen ray should point along camera forward.");

    EditableModel model;
    model.sections.push_back({
        1,
        cy::Matrix4f::Identity(),
        {cy::Vec3f(0.0f), 1.0f} });
    float distance = 0.0f;
    Require(HitTestEditableModel(ray, model, distance) && distance > 0.0f,
        "Center-screen ray should hit the model bounds.");

    model.transform.position.x = 20.0f;
    Require(!HitTestEditableModel(ray, model, distance),
        "Moved model bounds should no longer be hit by the old ray.");
}

void TestMergedWorldBounds()
{
    EditableModel model;
    model.transform.scale = cy::Vec3f(2.0f);
    model.sections.push_back({
        1,
        cy::Matrix4f::Translation(cy::Vec3f(-2.0f, 0.0f, 0.0f)),
        {cy::Vec3f(0.0f), 1.0f} });
    model.sections.push_back({
        2,
        cy::Matrix4f::Translation(cy::Vec3f(2.0f, 0.0f, 0.0f)),
        {cy::Vec3f(0.0f), 1.0f} });
    const PrimitiveBounds bounds = model.GetWorldBounds();
    Require(NearlyEqual(bounds.center.x, 0.0f) &&
            NearlyEqual(bounds.radius, 6.0f),
        "Merged bounds should include root scale and every model section.");
}

void TestEditorSelectionTransitions()
{
    EditorSelection selection;
    Require(selection.type == EditorSelectionType::None,
        "Editor selection should start empty.");

    selection.SelectModel();
    Require(selection.IsModelSelected() &&
            selection.lightId == InvalidLightId,
        "Selecting the model should clear any light identity.");

    selection.SelectLight(7);
    Require(!selection.IsModelSelected() && selection.IsLightSelected(7),
        "Selecting a light should replace the model selection.");

    selection.Clear();
    Require(selection.type == EditorSelectionType::None &&
            selection.lightId == InvalidLightId,
        "Clearing selection should reset both type and light identity.");
}

void TestModelAndLightSelectionAreDistinct()
{
    Camera camera(cy::Vec3f(0.0f), 10.0f);
    camera.SetAspectRatio(1.0f);

    EditableModel model;
    model.sections.push_back({
        1,
        cy::Matrix4f::Identity(),
        {cy::Vec3f(0.0f), 1.0f} });

    EditableLight light;
    light.proxy.id = 7;
    light.proxy.type = LightType::Point;
    light.transform.position = cy::Vec3f(0.0f);
    std::vector<EditableLight> lights{light};

    EditorPickResult pick = PickEditorObject(
        500.0f,
        500.0f,
        1000.0f,
        1000.0f,
        camera.GetProjectionMatrix(),
        camera.GetViewMatrix(),
        model,
        lights);
    Require(
        pick.type == EditorSelectionType::Light && pick.lightId == 7,
        "A light icon should take priority when it overlaps the model.");

    lights.front().transform.position.x = 20.0f;
    pick = PickEditorObject(
        500.0f,
        500.0f,
        1000.0f,
        1000.0f,
        camera.GetProjectionMatrix(),
        camera.GetViewMatrix(),
        model,
        lights);
    Require(
        pick.type == EditorSelectionType::Model,
        "The model should remain independently selectable away from light icons.");

    model.transform.position.x = 20.0f;
    pick = PickEditorObject(
        500.0f,
        500.0f,
        1000.0f,
        1000.0f,
        camera.GetProjectionMatrix(),
        camera.GetViewMatrix(),
        model,
        lights);
    Require(
        pick.type == EditorSelectionType::None,
        "Clicking empty space should clear both model and light selection.");
}
}

int main()
{
    TestTransformComposition();
    TestCenterRayAndModelHit();
    TestMergedWorldBounds();
    TestEditorSelectionTransitions();
    TestModelAndLightSelectionAreDistinct();
    std::cout << "Editor interaction tests passed." << std::endl;
    return EXIT_SUCCESS;
}
