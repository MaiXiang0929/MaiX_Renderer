// SPDX-License-Identifier: MIT
#include "Editor/Inspector/EnvironmentPanel.h"
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Core/Platform/Windows/FileDialog.h"
#include <imgui.h>
#include <string>
#include <exception>

void DrawEnvironmentControls(SceneState& scene, Renderer& renderer, void* nativeWindow)
{
    static std::string error;
    ImGui::SeparatorText("Environment Lighting");
    const auto label = scene.environmentSource.empty() ? std::string("Built-in sky") : scene.environmentSource.filename().u8string();
    ImGui::TextWrapped("Source: %s", label.c_str());
    if (!scene.environmentSource.empty() && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", scene.environmentSource.u8string().c_str());
    auto load = [&](std::filesystem::path source) {
        // Prepare the CPU path before any GPU commit; path swap after success is non-throwing.
        if (renderer.LoadEnvironment(source,error)) scene.environmentSource.swap(source);
    };
    if (ImGui::Button("Load HDR..."))
    {
        try
        {
            if (auto source=FileDialog::OpenHdrEnvironmentFile(nativeWindow))
                load(std::filesystem::absolute(*source).lexically_normal());
        }
        catch (const std::exception& exception) { error=exception.what(); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Use built-in sky")) load({});
    if (!scene.environmentSource.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button("Reload HDR")) load(scene.environmentSource);
    }
    ImGui::SliderFloat("Environment intensity",&scene.environment.intensity,0.0f,16.0f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Linear multiplier for the sky and PBR environment light; direct lights are unchanged.");
    ImGui::SliderFloat("Environment rotation",&scene.environment.rotationDegrees,-180.0f,180.0f,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Reset environment parameters")) scene.environment=EnvironmentSettings{};
    if (!error.empty()) ImGui::TextWrapped("Environment: %s",error.c_str());
}
