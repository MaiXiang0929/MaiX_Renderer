// SPDX-License-Identifier: MIT
#include "StartupScene.h"
#include "StartupSceneEmbedded.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace
{
using Json = nlohmann::json;
float Number(const Json &value)
{
    if (!value.is_number())
        throw std::runtime_error("Expected a number");
    const float result = value.get<float>();
    if (!std::isfinite(result))
        throw std::runtime_error("Non-finite number");
    return result;
}
cy::Vec3f Vector(const Json &value)
{
    if (!value.is_array() || value.size() != 3)
        throw std::runtime_error("Expected a 3-component vector");
    return {Number(value[0]), Number(value[1]), Number(value[2])};
}
std::string Name(const Json &value)
{
    auto result = value.get<std::string>();
    if (result.empty())
        throw std::runtime_error("Empty object name");
    return result;
}
} // namespace
bool ParseStartupScene(const std::string &text, StartupSceneDefinition &result, std::string &error)
{
    try
    {
        const auto json = Json::parse(text);
        if (!json.at("version").is_number_integer() || json.at("version") != 1)
            throw std::runtime_error("Unsupported startup scene version");
        StartupSceneDefinition candidate;
        const auto &cube = json.at("cube");
        candidate.cubeName = Name(cube.at("name"));
        candidate.cubeEdgeLength = Number(cube.at("edgeLength"));
        candidate.cubeTransform.position = Vector(cube.at("position"));
        candidate.cubeTransform.rotationDegrees = Vector(cube.at("rotation"));
        candidate.cubeTransform.scale = Vector(cube.at("scale"));
        if (candidate.cubeEdgeLength <= 0 || candidate.cubeEdgeLength > 10000 ||
            candidate.cubeTransform.scale.x < 0.001f || candidate.cubeTransform.scale.y < 0.001f ||
            candidate.cubeTransform.scale.z < 0.001f || candidate.cubeTransform.scale.Max() > 1000)
            throw std::runtime_error("Invalid cube size or scale");
        const auto &light = json.at("directionalLight");
        candidate.lightName = Name(light.at("name"));
        candidate.lightRotation = Vector(light.at("rotation"));
        candidate.lightColor = Vector(light.at("color"));
        candidate.lightIntensity = Number(light.at("intensity"));
        candidate.castsShadow = light.at("castsShadow").get<bool>();
        if (candidate.lightColor.x < 0 || candidate.lightColor.y < 0 ||
            candidate.lightColor.z < 0 || candidate.lightColor.Max() > 1 ||
            candidate.lightIntensity < 0 || candidate.lightIntensity > 100)
            throw std::runtime_error("Invalid light parameters");
        const auto &camera = json.at("camera");
        candidate.camera.name = Name(camera.at("name"));
        candidate.camera.transform.position = Vector(camera.at("position"));
        candidate.camera.transform.rotationDegrees = Vector(camera.at("rotation"));
        candidate.camera.fovDegrees = Number(camera.at("fovDegrees"));
        candidate.camera.nearPlane = Number(camera.at("nearPlane"));
        candidate.camera.farPlane = Number(camera.at("farPlane"));
        if (candidate.camera.fovDegrees < 1 || candidate.camera.fovDegrees > 150 ||
            candidate.camera.nearPlane < 0.0001f ||
            candidate.camera.farPlane <= candidate.camera.nearPlane)
            throw std::runtime_error("Invalid camera projection");
        candidate.selectedObject = json.at("selectedObject").get<std::string>();
        if (candidate.selectedObject != "cube" && candidate.selectedObject != "directionalLight" &&
            candidate.selectedObject != "camera")
            throw std::runtime_error("Unknown selected object");
        result = std::move(candidate);
        error.clear();
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}
StartupSceneDefinition LoadStartupScene(const std::filesystem::path &path, std::string &diagnostic)
{
    StartupSceneDefinition result;
    std::ifstream input(path);
    std::ostringstream text;
    text << input.rdbuf();
    if (input && ParseStartupScene(text.str(), result, diagnostic))
    {
        diagnostic = "Loaded " + path.generic_string();
        return result;
    }
    const auto reason = diagnostic.empty() ? "file unavailable" : diagnostic;
    if (!ParseStartupScene(EmbeddedStartupScene, result, diagnostic))
        throw std::runtime_error("Invalid embedded startup scene: " + diagnostic);
    diagnostic = "Embedded fallback (" + reason + ")";
    return result;
}
