// SPDX-License-Identifier: MIT
#include "ApplicationOptions.h"

#include <charconv>
#include <system_error>

namespace
{
bool ParseGridSize(const std::string& value, std::uint32_t& gridSize)
{
    if (value.empty())
        return false;

    const char* begin = value.data();
    const char* end = begin + value.size();
    const std::from_chars_result result = std::from_chars(begin, end, gridSize);
    return result.ec == std::errc() && result.ptr == end && gridSize > 0 &&
        gridSize <= MaximumInstanceGridSize;
}
}

bool ParseApplicationOptions(
    const std::vector<std::string>& arguments,
    ApplicationOptions& options,
    std::string& errorMessage)
{
    options = {};
    errorMessage.clear();
    std::vector<std::string> positionalArguments;

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string& argument = arguments[index];
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
            continue;
        }
        if (argument == "--instance-grid")
        {
            if (options.instanceGridSize != 0)
            {
                errorMessage = "--instance-grid may only be specified once.";
                return false;
            }
            if (index + 1 >= arguments.size() ||
                !ParseGridSize(arguments[++index], options.instanceGridSize))
            {
                errorMessage = "--instance-grid requires an integer from 1 to " +
                    std::to_string(MaximumInstanceGridSize) + ".";
                return false;
            }
            continue;
        }
        if (argument == "--material-lab")
        {
            if (options.materialLab)
            {
                errorMessage = "--material-lab may only be specified once.";
                return false;
            }
            options.materialLab = true;
            continue;
        }
        if (argument == "--translucency-test")
        {
            if (options.translucencyTest)
            {
                errorMessage =
                    "--translucency-test may only be specified once.";
                return false;
            }
            options.translucencyTest = true;
            continue;
        }
        if (!argument.empty() && argument.front() == '-')
        {
            errorMessage = "Unknown option: " + argument;
            return false;
        }
        positionalArguments.push_back(argument);
    }

    if (positionalArguments.size() > 2)
    {
        errorMessage = "Expected at most a normal map and a displacement map.";
        return false;
    }
    if (options.materialLab && options.instanceGridSize != 0)
    {
        errorMessage =
            "--material-lab and --instance-grid cannot be used together.";
        return false;
    }
    if (!positionalArguments.empty())
        options.normalMapPath = positionalArguments[0];
    if (positionalArguments.size() == 2)
        options.displacementMapPath = positionalArguments[1];
    return true;
}

const char* GetApplicationUsage()
{
    return
        "Usage: OpenGL_Project [normal.png] [displacement.png] "
        "[--instance-grid N] [--material-lab] [--translucency-test]\n"
        "  --instance-grid N  Submit an N x N shared-resource benchmark grid "
        "(1-32).\n"
        "  --material-lab     Show four PBR reference materials using shared "
        "geometry.\n"
        "  --translucency-test  Add the three-plane transparency test scene.";
}
