// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Core/ApplicationOptions.h"
#include "Core/InstanceGrid.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "[ApplicationBenchmarkTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

bool NearlyEqual(float left, float right)
{
    return std::abs(left - right) < 1.0e-5f;
}

void TestDefaultOptions()
{
    ApplicationOptions options;
    std::string error;
    Require(ParseApplicationOptions({}, options, error),
        "Empty arguments should use the default scene.");
    Require(options.instanceGridSize == 0,
        "The benchmark must be disabled by default.");
    Require(!options.materialLab,
        "The material lab must be disabled by default.");
    Require(!options.translucencyTest,
        "The translucency test must be disabled by default.");
    Require(options.normalMapPath.empty() && options.displacementMapPath.empty(),
        "Default map paths should be resolved by the application entry point.");
}

void TestTranslucencyTestOption()
{
    ApplicationOptions options;
    std::string error;
    Require(ParseApplicationOptions(
        {"--translucency-test"}, options, error),
        "The translucency test option should be accepted.");
    Require(options.translucencyTest,
        "The translucency test option was not parsed correctly.");
    Require(!ParseApplicationOptions(
        {"--translucency-test", "--translucency-test"}, options, error),
        "Duplicate translucency test options must be rejected.");
}

void TestMaterialLabOptions()
{
    ApplicationOptions options;
    std::string error;
    Require(ParseApplicationOptions(
        {"normal.png", "--material-lab"}, options, error),
        "The material lab should preserve a supplied normal map path.");
    Require(options.materialLab && options.normalMapPath == "normal.png",
        "The material lab option was not parsed correctly.");
    Require(!ParseApplicationOptions(
        {"--material-lab", "--instance-grid", "8"}, options, error),
        "The material lab and instance benchmark must be mutually exclusive.");
    Require(!ParseApplicationOptions(
        {"--material-lab", "--material-lab"}, options, error),
        "Duplicate material lab options must be rejected.");
}

void TestGridAndMaterialMapOptions()
{
    ApplicationOptions options;
    std::string error;
    Require(ParseApplicationOptions(
        {"normal.png", "--instance-grid", "16", "displacement.png"},
        options,
        error),
        "Grid options should coexist with the legacy positional map paths.");
    Require(options.instanceGridSize == 16,
        "The requested grid size was not parsed.");
    Require(options.normalMapPath == "normal.png" &&
        options.displacementMapPath == "displacement.png",
        "Positional material map paths changed during parsing.");
}

void TestInvalidOptions()
{
    ApplicationOptions options;
    std::string error;
    Require(!ParseApplicationOptions({"--instance-grid", "0"}, options, error),
        "A zero-sized benchmark grid must be rejected.");
    Require(!ParseApplicationOptions({"--instance-grid", "33"}, options, error),
        "A grid above the safety limit must be rejected.");
    Require(!ParseApplicationOptions({"--instance-grid"}, options, error),
        "A missing grid value must be rejected.");
    Require(!ParseApplicationOptions({"--unknown"}, options, error),
        "Unknown options must be rejected.");
    Require(!ParseApplicationOptions(
        {"--instance-grid", "8", "--instance-grid", "16"},
        options,
        error),
        "Duplicate grid options must be rejected.");
}

void TestCenteredGridLayout()
{
    const std::vector<cy::Vec3f> one = BuildInstanceGridOffsets(1, 2.0f);
    Require(one.size() == 1 && NearlyEqual(one[0].x, 0.0f) &&
        NearlyEqual(one[0].y, 0.0f),
        "A 1x1 grid should contain one centered instance.");

    const std::vector<cy::Vec3f> two = BuildInstanceGridOffsets(2, 2.0f);
    Require(two.size() == 4,
        "A 2x2 grid should contain four instances.");
    Require(NearlyEqual(two.front().x, -1.0f) &&
        NearlyEqual(two.front().y, -1.0f) &&
        NearlyEqual(two.back().x, 1.0f) &&
        NearlyEqual(two.back().y, 1.0f),
        "Grid endpoints should be centered around the world origin.");

    cy::Vec3f sum(0.0f, 0.0f, 0.0f);
    for (const cy::Vec3f& offset : two)
        sum += offset;
    Require(NearlyEqual(sum.Length(), 0.0f),
        "The complete grid should have a zero-mean translation.");
}

void TestSceneRadius()
{
    Require(NearlyEqual(
        CalculateInstanceGridSceneRadius(1, 2.0f, 0.5f), 0.5f),
        "A 1x1 scene radius should equal the instance radius.");
    Require(NearlyEqual(
        CalculateInstanceGridSceneRadius(2, 2.0f, 0.5f),
        std::sqrt(2.0f) + 0.5f),
        "The scene radius should contain the corner instance sphere.");
    Require(BuildInstanceGridOffsets(0, 2.0f).empty(),
        "A zero-sized layout should not produce instances.");
}
}

int main()
{
    TestDefaultOptions();
    TestGridAndMaterialMapOptions();
    TestMaterialLabOptions();
    TestTranslucencyTestOption();
    TestInvalidOptions();
    TestCenteredGridLayout();
    TestSceneRadius();
    std::cout << "Application benchmark tests passed." << std::endl;
    return EXIT_SUCCESS;
}
