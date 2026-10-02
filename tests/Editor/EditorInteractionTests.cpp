// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "Core/Camera.h"
#include "Core/Transform.h"
#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"
#include "Editor/EditorSelection.h"
#include "Editor/EditorMaterialSelection.h"
#include "Editor/EditorValueConstraints.h"
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

    selection.SelectModel(ModelId{11});
    Require(selection.IsModelSelected(ModelId{11}) &&
            !selection.IsModelSelected(ModelId{12}) &&
            selection.lightId == InvalidLightId,
        "Selecting a model should retain its identity and clear light identity.");

    selection.SelectLight(7);
    Require(!selection.IsModelSelected(ModelId{11}) &&
            selection.IsLightSelected(7),
        "Selecting a light should replace the model selection.");

    selection.Clear();
    Require(selection.type == EditorSelectionType::None &&
            !selection.modelId.IsValid() &&
            selection.lightId == InvalidLightId,
        "Clearing selection should reset type and object identities.");
}

void TestCentimeterRootBoundsAndPicking()
{
    EditableModel model;
    model.transform.position = cy::Vec3f(0.3f, 0.0f, 0.0f);
    model.transform.scale = cy::Vec3f(0.01f);
    const cy::Vec3f rawCenter(5.0f, 0.0f, 0.0f);
    model.sections.push_back({1,
        cy::Matrix4f::Translation(cy::Vec3f(20.0f, 0.0f, 0.0f) - rawCenter),
        {rawCenter, 10.0f}});
    const PrimitiveBounds bounds = model.GetWorldBounds();
    Require(NearlyEqual(bounds.center.x, 0.5f) && NearlyEqual(bounds.radius, 0.1f),
        "Root unit scale must convert raw grid offsets and bounds exactly once.");
    WorldRay ray;
    ray.origin = cy::Vec3f(0.5f, 0.0f, 1.0f);
    ray.direction = cy::Vec3f(0.0f, 0.0f, -1.0f);
    float distance = 0.0f;
    Require(HitTestEditableModel(ray, model, distance), "Picking must use meter-space world bounds.");
    ray.origin.x = 0.7f;
    Require(!HitTestEditableModel(ray, model, distance), "Picking must not retain the raw large radius.");
}

void TestModelAndLightSelectionAreDistinct()
{
    Camera camera(cy::Vec3f(0.0f), 10.0f);
    camera.SetAspectRatio(1.0f);

    EditableModel model;
    model.id = ModelId{11};
    model.sections.push_back({
        1,
        cy::Matrix4f::Identity(),
        {cy::Vec3f(0.0f), 1.0f} });

    EditableLight light;
    light.proxy.id = 7;
    light.proxy.type = LightType::Point;
    light.transform.position = cy::Vec3f(0.0f);
    std::vector<EditableLight> lights{light};
    std::vector<EditableModel> models{model};

    EditorPickResult pick = PickEditorObject(
        500.0f,
        500.0f,
        1000.0f,
        1000.0f,
        camera.GetProjectionMatrix(),
        camera.GetViewMatrix(),
        models,
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
        models,
        lights);
    Require(
        pick.type == EditorSelectionType::Model && pick.modelId == model.id,
        "The model should remain independently selectable away from light icons.");

    models.front().transform.position.x = 20.0f;
    pick = PickEditorObject(
        500.0f,
        500.0f,
        1000.0f,
        1000.0f,
        camera.GetProjectionMatrix(),
        camera.GetViewMatrix(),
        models,
        lights);
    Require(
        pick.type == EditorSelectionType::None,
        "Clicking empty space should clear both model and light selection.");
}

void TestMultipleModelPickingAndBounds()
{
    Camera camera(cy::Vec3f(0.0f), 10.0f);
    camera.SetAspectRatio(1.0f);

    EditableModel back;
    back.id = ModelId{21};
    back.sections.push_back({
        1, cy::Matrix4f::Identity(), {cy::Vec3f(0.0f), 1.0f} });
    back.transform.position.z = -2.0f;

    EditableModel front;
    front.id = ModelId{22};
    front.sections.push_back({
        2, cy::Matrix4f::Identity(), {cy::Vec3f(0.0f), 1.0f} });
    front.transform.position.z = 2.0f;

    std::vector<EditableModel> models{back, front};
    const std::vector<EditableLight> noLights;
    const auto pickCenter = [&]()
    {
        return PickEditorObject(
            500.0f, 500.0f, 1000.0f, 1000.0f,
            camera.GetProjectionMatrix(), camera.GetViewMatrix(),
            models, noLights);
    };

    Require(pickCenter().modelId == front.id,
        "Picking overlapping models should select the nearest model ID.");
    const PrimitiveBounds sceneBounds = GetSceneWorldBounds(models);
    Require(NearlyEqual(sceneBounds.center.z, 0.0f) &&
            NearlyEqual(sceneBounds.radius, 3.0f),
        "Scene bounds should contain both independently positioned models.");

    FindEditableModel(models, front.id)->transform.position.x = 20.0f;
    Require(pickCenter().modelId == back.id,
        "Moving one model should expose the other model to picking.");
    models.erase(models.begin() + 1);
    Require(FindEditableModel(models, front.id) == nullptr &&
            FindEditableModel(models, back.id) != nullptr,
        "Removing one model should preserve the other model identity.");
}

void TestUniqueModelNamesAfterRemoval()
{
    std::vector<EditableModel> models;
    EditableModel first;
    first.id = ModelId{2};
    first.name = MakeUniqueModelName(models, "Avatar");
    models.push_back(first);
    Require(first.name == "Avatar", "First import should keep its filename stem.");

    EditableModel second;
    second.id = ModelId{3};
    second.name = MakeUniqueModelName(models, "Avatar");
    models.push_back(second);
    Require(second.name == "Avatar_001",
        "A duplicate filename should receive the first padded suffix.");

    EditableModel third;
    third.id = ModelId{4};
    third.name = MakeUniqueModelName(models, "Avatar");
    models.push_back(third);
    Require(third.name == "Avatar_002",
        "A further duplicate should skip names already in the scene.");

    models.erase(models.begin() + 1);
    EditableModel replacement;
    replacement.id = ModelId{5};
    replacement.name = MakeUniqueModelName(models, "Avatar");
    models.push_back(replacement);
    Require(replacement.name == "Avatar_001" &&
            replacement.id != second.id,
        "Removing a model may reuse its display name without reusing its ID.");
}

void TestMaterialSelectionAcrossModelsAndDeletion()
{
    EditableModel first;
    first.id = ModelId{11};
    first.name = "Avatar";
    first.sections.push_back({1, cy::Matrix4f::Identity(), {}, MaterialHandle{10}, "Body"});
    first.sections.push_back({2, cy::Matrix4f::Identity(), {}, MaterialHandle{20}, "Face"});
    first.sections.push_back({3, cy::Matrix4f::Identity(), {}, MaterialHandle{10}, "Hair"});
    EditableModel second = first;
    second.id = ModelId{12};
    second.name = "Avatar_001";
    for (auto& section : second.sections)
        section.material.id += 20;
    std::vector<EditableModel> models{first, second};
    std::vector<MaterialHandle> live{{10}, {20}, {30}, {40}};
    EditorSelection objectSelection;
    EditorMaterialSelection materialSelection;
    objectSelection.SelectModel(first.id);
    auto candidates = materialSelection.Synchronize(objectSelection, models, live);
    Require(candidates.size() == 2 && materialSelection.Material().id == 10,
        "Model material candidates should deduplicate shared section resources.");
    materialSelection.Select(first.id, MaterialHandle{20});
    materialSelection.Synchronize(objectSelection, models, live);
    Require(materialSelection.Material().id == 20,
        "Synchronizing the same model should retain its chosen material handle.");

    objectSelection.SelectModel(second.id);
    materialSelection.Synchronize(objectSelection, models, live);
    Require(materialSelection.Owner() == second.id && materialSelection.Material().id == 30,
        "Switching duplicate imports should select a material belonging to the new model.");
    models.erase(models.begin());
    live.erase(live.begin(), live.begin() + 2);
    materialSelection.Synchronize(objectSelection, models, live);
    Require(materialSelection.Material().id == 30,
        "Deleting preceding list entries should not change the selected resource.");
    live.erase(live.begin());
    candidates = materialSelection.Synchronize(objectSelection, models, live);
    Require(candidates.size() == 1 && !materialSelection.Material().IsValid(),
        "An invalid material should clear selection without silently selecting a different one.");
    models.clear();
    materialSelection.Synchronize(objectSelection, models, live);
    Require(!materialSelection.Owner().IsValid() && !materialSelection.Material().IsValid(),
        "Deleting the owner should clear both material and model identities.");

    materialSelection.SetAllMaterials(true);
    materialSelection.Synchronize(objectSelection, models, live);
    Require(materialSelection.Material().id == 40,
        "Explicit debug scope should allow editing materials without a model owner.");
    live.push_back(MaterialHandle{50});
    live.erase(live.begin());
    materialSelection.Synchronize(objectSelection, models, live);
    Require(!materialSelection.Material().IsValid(),
        "Deleting the globally selected resource should not retarget another live material.");
    models.push_back(first);
    live.push_back(MaterialHandle{10});
    objectSelection.SelectModel(first.id);
    materialSelection.Select(first.id, MaterialHandle{10});
    materialSelection.Synchronize(objectSelection, models, live);
    Require(!materialSelection.AllMaterials() && materialSelection.Material().id == 10,
        "Inspector material selection should leave debug scope and target the model.");
    objectSelection.SelectLight(7);
    materialSelection.Synchronize(objectSelection, models, live);
    Require(!materialSelection.Material().IsValid(),
        "Selecting a light should clear model-scoped material editing.");
}

void TestEditorValueConstraints()
{
    cy::Vec3f scale(-2.0f, 0.0f, 2000.0f);
    EditorValueConstraints::SanitizeScale(scale);
    Require(NearlyEqual(scale.x, 2.0f) &&
            NearlyEqual(scale.y, EditorValueConstraints::MinimumScale) &&
            NearlyEqual(scale.z, EditorValueConstraints::MaximumScale),
        "Inspector scale constraints should prevent mirrored or degenerate transforms.");

    cy::Vec3f direction(0.0f);
    EditorValueConstraints::SanitizeDirection(direction);
    Require(NearlyEqual(direction.x, 0.0f) &&
            NearlyEqual(direction.y, -1.0f) &&
            NearlyEqual(direction.z, 0.0f),
        "A zero light direction should fall back to world down.");

    float intensity = -2.0f;
    float range = 0.0f;
    EditorValueConstraints::SanitizeLightScalars(intensity, range);
    Require(NearlyEqual(intensity, 0.0f) &&
            NearlyEqual(range, EditorValueConstraints::MinimumLightRange),
        "Light scalar constraints should reject negative intensity and zero range.");

    float innerCone = 75.0f;
    float outerCone = 30.0f;
    EditorValueConstraints::SanitizeSpotConeDegrees(innerCone, outerCone);
    Require(NearlyEqual(innerCone, 30.0f) && NearlyEqual(outerCone, 30.0f),
        "Spot cone constraints should keep the inner cone inside the outer cone.");
}
}

int main()
{
    TestTransformComposition();
    TestCenterRayAndModelHit();
    TestMergedWorldBounds();
    TestCentimeterRootBoundsAndPicking();
    TestEditorSelectionTransitions();
    TestModelAndLightSelectionAreDistinct();
    TestMultipleModelPickingAndBounds();
    TestUniqueModelNamesAfterRemoval();
    TestEditorValueConstraints();
    TestMaterialSelectionAcrossModelsAndDeletion();
    std::cout << "Editor interaction tests passed." << std::endl;
    return EXIT_SUCCESS;
}
