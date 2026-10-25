// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Core/Math/Transform.h"
#include "Engine/Runtime/Scene/SceneCamera.h"
#include <filesystem>
#include <optional>
#include <string>
// Fixed records for the two models used by the basic startup scene.
struct StartupModelDefinition
{
    std::string name;
    std::filesystem::path path;
    Transform transform;
    cy::Vec3f baseColor{1.0f}; // Linear after parsing the legacy startup definition.
    float roughness = 0.9f;
};
struct BasicStartupSceneDefinition
{
    StartupModelDefinition ground, monkey;
    std::string pointLightName;
    cy::Vec3f pointLightPosition, pointLightColor;
    float pointLightIntensity = 0, pointLightRange = 0;
};
struct StartupSceneDefinition
{
    // Cube/directional settings are retained for the existing validation modes.
    std::string cubeName;
    float cubeEdgeLength = 0;
    Transform cubeTransform;
    std::string lightName;
    cy::Vec3f lightRotation, lightColor;
    float lightIntensity = 0;
    bool castsShadow = false;
    SceneCamera camera;
    std::string selectedObject;
    std::optional<BasicStartupSceneDefinition> basicScene;
};
bool ParseStartupScene(const std::string &text, StartupSceneDefinition &result, std::string &error);
StartupSceneDefinition LoadStartupScene(const std::filesystem::path &path, std::string &diagnostic);
