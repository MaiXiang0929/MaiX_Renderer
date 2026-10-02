// SPDX-License-Identifier: MIT
#include "Assets/Geometry/CubeGeometry.h"
#include "Core/Camera.h"
#include "Core/DirectionalShadow.h"
#include "Core/StartupScene.h"
#include "Editor/EditorSelection.h"
#include "StartupSceneEmbedded.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

void Require(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << "[StartupSceneTests] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
bool Close(float a, float b)
{
    return std::abs(a - b) < 1e-4f;
}
int main()
{
    StartupSceneDefinition scene;
    std::string error;
    Require(ParseStartupScene(EmbeddedStartupScene, scene, error), "Embedded default must parse");
    Require(Close(scene.cubeEdgeLength, 2) && scene.cubeTransform.position.LengthSquared() == 0 &&
                scene.cubeTransform.rotationDegrees.LengthSquared() == 0 &&
                scene.cubeTransform.scale == cy::Vec3f(1),
            "Default cube must be 2 meters at origin with identity transform");
    auto geometry = CreateCubeGeometry(scene.cubeEdgeLength);
    Require(geometry.vertices.size() == 24 && geometry.indices.size() == 36, "Cube topology");
    for (const auto &v : geometry.vertices)
    {
        Require(Close(std::abs(v.Position.x), 1) && Close(std::abs(v.Position.y), 1) &&
                    Close(std::abs(v.Position.z), 1),
                "Cube corners must lie on +/-1");
        cy::Vec3f tangent(v.Tangent.x, v.Tangent.y, v.Tangent.z);
        Require(Close(v.Normal.Length(), 1) && Close(tangent.Length(), 1) &&
                    Close(v.Normal.Dot(tangent), 0) && v.Tangent.w == 1,
                "Orthonormal tangent frame");
    }
    for (size_t i = 0; i < geometry.indices.size(); i += 3)
    {
        const auto &a = geometry.vertices[geometry.indices[i]];
        const auto &b = geometry.vertices[geometry.indices[i + 1]];
        const auto &c = geometry.vertices[geometry.indices[i + 2]];
        Require((b.Position - a.Position).Cross(c.Position - a.Position).Dot(a.Normal) > 0,
                "Outward CCW winding");
    }
    const auto originalName = scene.cubeName;
    for (auto text : {"{}", "[]", "{\"version\":2}", "not json"})
        Require(!ParseStartupScene(text, scene, error) && scene.cubeName == originalName,
                "Invalid parse must preserve destination");
    auto invalid = std::string(EmbeddedStartupScene);
    auto replace = [&](std::string from, std::string to) {
        auto p = invalid.find(from);
        Require(p != std::string::npos, "Test fixture field");
        invalid.replace(p, from.size(), to);
    };
    replace("\"nearPlane\": 0.1", "\"nearPlane\": -1");
    Require(!ParseStartupScene(invalid, scene, error), "Negative clip plane must be rejected");
    const auto temp = std::filesystem::temp_directory_path() / "maix-startup-fallback-test.json";
    {
        std::ofstream output(temp);
        output << "{ invalid";
    }
    auto fallback = LoadStartupScene(temp, error);
    Require(fallback.cubeName == originalName &&
                error.find("Embedded fallback") != std::string::npos,
            "Invalid external scene must fall back");
    std::filesystem::remove(temp);
    fallback = LoadStartupScene(temp, error);
    Require(Close(fallback.cubeEdgeLength, 2), "Missing external scene must fall back");
    const auto view = scene.camera.GetViewMatrix();
    const auto pos = scene.camera.transform.position;
    auto eye = view * cy::Vec4f(pos.x, pos.y, pos.z, 1);
    Require(Close(eye.x, 0) && Close(eye.y, 0) && Close(eye.z, 0),
            "Scene camera view is inverse rigid pose");
    const auto target = view * cy::Vec4f(0, 0, 0, 1);
    Require(std::abs(target.x) < 0.001f && std::abs(target.y) < 0.001f && target.z < 0,
            "Startup camera aims at origin");
    const auto projection = scene.camera.GetProjectionMatrix(2);
    const auto near = projection * cy::Vec4f(0, 0, -scene.camera.nearPlane, 1);
    const auto far = projection * cy::Vec4f(0, 0, -scene.camera.farPlane, 1);
    Require(Close(near.z / near.w, -1) && Close(far.z / far.w, 1),
            "Scene camera clips use authored planes");
    Camera editor;
    editor.ProcessMouseOrbit(20, 30);
    editor.ProcessMousePan(2, 3, 600);
    editor.ProcessMouseZoom(20);
    Require(scene.camera.transform.position == pos && Close(scene.camera.nearPlane, 0.1f),
            "Editor navigation preserves scene camera");
    EditorSelection selection;
    selection.SelectModel(ModelId{3});
    selection.SelectCamera(CameraId{1});
    Require(selection.IsCameraSelected(CameraId{1}) && !selection.modelId.IsValid() &&
                selection.lightId == InvalidLightId,
            "Camera selection clears model and light");
    selection.SelectLight(2);
    Require(!selection.cameraId.IsValid(), "Light selection clears camera");
    for (auto direction : {cy::Vec3f(0, -1, 0), cy::Vec3f(1, 2, -3), cy::Vec3f(0, 0, 0)})
    {
        PrimitiveBounds bounds;
        bounds.center = {13, -5, 8};
        bounds.radius = 3;
        auto vp = FitDirectionalShadow(bounds, direction, 0.4f);
        for (auto axis : {cy::Vec3f(1, 0, 0), cy::Vec3f(0, 1, 0), cy::Vec3f(0, 0, 1)})
            for (float sign : {-1.f, 1.f})
            {
                const auto p = bounds.center + axis * (3.4f * sign);
                auto clip = vp * cy::Vec4f(p.x, p.y, p.z, 1);
                Require(std::isfinite(clip.x) && std::abs(clip.x) <= clip.w &&
                            std::abs(clip.y) <= clip.w && std::abs(clip.z) <= clip.w,
                        "Directional projection contains padded sphere extremes");
            }
    }
    std::cout << "Startup scene, geometry, camera, selection and shadow checks passed\n";
}
