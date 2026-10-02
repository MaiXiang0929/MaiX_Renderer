// SPDX-License-Identifier: MIT
#pragma once
#include "Core/Transform.h"
#include "Editor/EditableCamera.h"
#include <filesystem>
#include <string>
struct StartupSceneDefinition
{
    std::string cubeName;
    float cubeEdgeLength = 0;
    Transform cubeTransform;
    std::string lightName;
    cy::Vec3f lightRotation, lightColor;
    float lightIntensity = 0;
    bool castsShadow = false;
    EditableCamera camera;
    std::string selectedObject;
};
bool ParseStartupScene(const std::string &text, StartupSceneDefinition &result, std::string &error);
StartupSceneDefinition LoadStartupScene(const std::filesystem::path &path, std::string &diagnostic);
