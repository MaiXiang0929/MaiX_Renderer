// SPDX-License-Identifier: MIT
#include "RendererStatisticsPanel.h"

#include <array>

#include <imgui.h>

#include "Renderer/Core/Renderer.h"

namespace
{
constexpr std::array<const char*, 8> PassNames = {
    "Shadow", "Reflection", "Forward", "Translucency", "Editor", "Bloom",
    "PostProcess", "Present"
};
}

void RendererStatisticsPanel::Draw(Renderer& renderer)
{
    ImGui::SetNextWindowSize(ImVec2(860.0f, 680.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Renderer Statistics"))
    {
        ImGui::End();
        return;
    }

    const Renderer::StatisticsSnapshot& scene = renderer.GetStatisticsSnapshot();
    ImGui::Text("Scene: %zu source  %zu visible  %zu culled",
        scene.sourcePrimitiveCount,
        scene.visiblePrimitiveCount,
        scene.culledPrimitiveCount);
    ImGui::Text("Draw lists: %zu opaque in %zu batches  %zu translucent",
        scene.opaqueDrawCount,
        scene.opaqueBatchCount,
        scene.translucentDrawCount);
    ImGui::Text("Groups: %zu shader  %zu material  %zu mesh",
        scene.shaderGroupCount, scene.materialGroupCount, scene.meshGroupCount);
    ImGui::Text("Resources: %zu meshes  %zu materials",
        scene.meshResourceCount, scene.materialResourceCount);

    ImGui::SeparatorText("CPU Submission");
    const RenderSubmissionSnapshot submission = renderer.GetSubmissionSnapshot();
    if (submission.valid && ImGui::BeginTable("CpuStats", 7,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("Draws / instanced");
        ImGui::TableSetupColumn("Instances");
        ImGui::TableSetupColumn("Shader req/change");
        ImGui::TableSetupColumn("Material req/change");
        ImGui::TableSetupColumn("Mesh req/change");
        ImGui::TableSetupColumn("Texture req/change");
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < PassNames.size(); ++index)
        {
            const PassSubmissionStats& stats = submission.passes[index];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(PassNames[index]);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%zu / %zu", stats.drawCalls, stats.instancedDrawCalls);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%zu", stats.submittedInstances);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%zu / %zu", stats.shaderBindRequests, stats.shaderChanges);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%zu / %zu", stats.materialBindRequests, stats.materialChanges);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%zu / %zu", stats.meshBindRequests, stats.meshChanges);
            ImGui::TableSetColumnIndex(6); ImGui::Text("%zu / %zu", stats.textureBindRequests, stats.textureChanges);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("GPU Timing");
    const GpuTimingSnapshot& gpu = renderer.GetGpuTimingSnapshot();
    if (ImGui::BeginTable("GpuStats", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("Last (ms)");
        ImGui::TableSetupColumn("EMA (ms)");
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < PassNames.size(); ++index)
        {
            const GpuPassTiming& timing = gpu.passes[index];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(PassNames[index]);
            ImGui::TableSetColumnIndex(1); ImGui::Text(timing.valid ? "%.3f" : "--", timing.lastMilliseconds);
            ImGui::TableSetColumnIndex(2); ImGui::Text(timing.valid ? "%.3f" : "--", timing.averageMilliseconds);
        }
        ImGui::EndTable();
    }
    ImGui::Text("Total: %.3f ms last  %.3f ms EMA", gpu.totalLastMilliseconds, gpu.totalAverageMilliseconds);
    ImGui::Text("Frames: %llu resolved  %llu skipped",
        static_cast<unsigned long long>(gpu.resolvedFrameCount),
        static_cast<unsigned long long>(gpu.skippedFrameCount));

    ImGui::SeparatorText("Render Controls");
    bool shadows = renderer.IsShadowsEnabled();
    if (ImGui::Checkbox("Shadows", &shadows)) renderer.SetShadowsEnabled(shadows);
    bool editor = renderer.AreEditorPrimitivesEnabled();
    if (ImGui::Checkbox("Editor primitives", &editor)) renderer.SetEditorPrimitivesEnabled(editor);
    bool toneMapping = renderer.IsToneMappingEnabled();
    if (ImGui::Checkbox("Tone mapping", &toneMapping)) renderer.SetToneMappingEnabled(toneMapping);
    bool bloom = renderer.IsBloomEnabled();
    if (ImGui::Checkbox("Bloom", &bloom)) renderer.SetBloomEnabled(bloom);
    if (bloom)
    {
        float threshold = renderer.GetBloomThreshold();
        if (ImGui::SliderFloat(
                "Bloom threshold", &threshold,
                MinimumBloomThreshold, MaximumBloomThreshold, "%.2f"))
            renderer.SetBloomThreshold(threshold);
        float intensity = renderer.GetBloomIntensity();
        if (ImGui::SliderFloat(
                "Bloom intensity", &intensity,
                MinimumBloomIntensity, MaximumBloomIntensity, "%.2f"))
            renderer.SetBloomIntensity(intensity);
    }
    float exposure = renderer.GetExposureCompensation();
    if (ImGui::SliderFloat(
            "Exposure (EV)",
            &exposure,
            MinimumExposureCompensation,
            MaximumExposureCompensation,
            "%+.2f"))
        renderer.SetExposureCompensation(exposure);
    bool tessellation = renderer.IsTessellationEnabled();
    if (ImGui::Checkbox("Tessellation", &tessellation)) renderer.SetTessellationEnabled(tessellation);
    bool wireframe = renderer.IsTessellationWireframe();
    if (ImGui::Checkbox("Wireframe", &wireframe)) renderer.SetTessellationWireframe(wireframe);
    float level = renderer.GetTessellationLevel();
    if (ImGui::SliderFloat("Tessellation level", &level, 1.0f, 64.0f, "%.0f")) renderer.SetTessellationLevel(level);
    float displacement = renderer.GetDisplacementScale();
    if (ImGui::SliderFloat("Displacement scale", &displacement, 0.0f, 2.0f, "%.3f")) renderer.SetDisplacementScale(displacement);

    ImGui::End();
}
