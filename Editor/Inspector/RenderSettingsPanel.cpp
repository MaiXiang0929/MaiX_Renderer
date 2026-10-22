// SPDX-License-Identifier: MIT
#include "Editor/Inspector/RenderSettingsPanel.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include <imgui.h>

void DrawRenderSettingsPanel(PostProcessSettings& settings)
{
    ImGui::SetNextWindowSize(ImVec2(390, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Render Settings"))
    {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Saved with the current scene.");
    if (ImGui::Button("Reset to Defaults")) settings = PostProcessSettings{};

    ImGui::SeparatorText("Exposure and Tone Mapping");
    ImGui::Checkbox("Tone mapping (ACES)", &settings.toneMappingEnabled);
    ImGui::SliderFloat("Exposure (EV)", &settings.exposureCompensation,
        MinimumExposureCompensation, MaximumExposureCompensation, "%+.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("+1 EV doubles brightness; -1 EV halves it.");

    ImGui::SeparatorText("Bloom");
    ImGui::Checkbox("Bloom", &settings.bloomEnabled);
    ImGui::BeginDisabled(!settings.bloomEnabled);
    ImGui::SliderFloat("Bloom threshold", &settings.bloomThreshold,
        MinimumBloomThreshold, MaximumBloomThreshold, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Threshold in linear HDR scene color, before exposure.");
    ImGui::SliderFloat("Bloom intensity", &settings.bloomIntensity,
        MinimumBloomIntensity, MaximumBloomIntensity, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();

    ImGui::SeparatorText("Screen Space Ambient Occlusion");
    ImGui::Checkbox("SSAO", &settings.ssaoEnabled);
    ImGui::BeginDisabled(!settings.ssaoEnabled);
    ImGui::SliderFloat("SSAO radius", &settings.ssaoRadius, MinimumSsaoRadius, MaximumSsaoRadius,
        "%.4f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sampling radius in scene world units.");
    ImGui::SliderFloat("SSAO intensity", &settings.ssaoIntensity, MinimumSsaoIntensity, MaximumSsaoIntensity,
        "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("SSAO bias", &settings.ssaoBias, MinimumSsaoBias, MaximumSsaoBias,
        "%.5f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Depth bias in scene world units; reduces self-occlusion.");
    ImGui::EndDisabled();
    ImGui::End();
}
