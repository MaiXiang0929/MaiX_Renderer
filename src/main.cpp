// SPDX-License-Identifier: MIT
#include "Core/Application.h"
#include "Core/ApplicationOptions.h"

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int index = 1; index < argc; ++index)
        arguments.emplace_back(argv[index]);

    ApplicationOptions options;
    std::string errorMessage;
    if (!ParseApplicationOptions(arguments, options, errorMessage))
    {
        std::cerr << errorMessage << '\n' << GetApplicationUsage() << std::endl;
        return 1;
    }
    if (options.showHelp)
    {
        std::cout << GetApplicationUsage() << std::endl;
        return 0;
    }

    if (options.normalMapPath.empty()) {
        std::cout << "No material maps supplied; loading the bundled teapot maps."
                  << std::endl;
        if (std::filesystem::exists("assets/models/teapot_normal.png"))
            options.normalMapPath = "assets/models/teapot_normal.png";
        if (std::filesystem::exists("assets/models/teapot_disp.png"))
            options.displacementMapPath = "assets/models/teapot_disp.png";
    }

    Application app(
        options.normalMapPath,
        options.displacementMapPath,
        options.instanceGridSize,
        options.materialLab,
        options.translucencyTest,
        options.faceShadowDemoModelPath,
        options.faceShadowDemoTexturePath,
        options.faceShadowDemoMaterialName);
    app.Run();
    return 0;
}
