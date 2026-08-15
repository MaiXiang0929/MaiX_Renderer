// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>

#include "Renderer/Pipeline/RenderPass.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[RenderPassContractTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}
}

int main()
{
    Require(static_cast<std::size_t>(RenderPassType::Shadow) == 0 &&
        static_cast<std::size_t>(RenderPassType::Forward) == 2 &&
        static_cast<std::size_t>(RenderPassType::Outline) == 3 &&
        static_cast<std::size_t>(RenderPassType::Translucency) == 4 &&
        static_cast<std::size_t>(RenderPassType::Present) == 8,
        "Pass enum order must match pipeline diagnostics ordering.");
    Require(static_cast<std::size_t>(RenderPassType::Count) == 9,
        "Pass diagnostics must reserve one slot per executable pass.");

    std::cout << "Render pass contract tests passed." << std::endl;
    return EXIT_SUCCESS;
}
