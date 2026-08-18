// SPDX-License-Identifier: MIT
#include "Editor/AssetImportPanel.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <utility>

#include <imgui.h>

#include "Assets/Import/FbxModelImporter.h"
#include "Platform/Windows/FileDialog.h"

namespace
{
std::string Lowercase(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}
}

AssetImportPanel::~AssetImportPanel()
{
    // A std::async future waits here if shutdown happens during a parse. The
    // importer only owns CPU memory and does not access Application state.
    if (m_ImportFuture.valid())
        m_ImportFuture.wait();
}

void AssetImportPanel::Update()
{
    if (m_Status != Status::Parsing || !m_ImportFuture.valid())
        return;

    if (m_ImportFuture.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    AssetImport::ModelImportResult result;
    try
    {
        result = m_ImportFuture.get();
    }
    catch (const std::exception& exception)
    {
        m_Status = Status::Error;
        m_Message = std::string("Model parsing failed: ") + exception.what();
        m_Warnings.clear();
        return;
    }
    if (!result)
    {
        m_Status = Status::Error;
        m_Message = result.error.empty()
            ? "Model parsing failed."
            : std::move(result.error);
        m_Warnings.clear();
        return;
    }

    m_Warnings = result.model.warnings;
    m_Message = "CPU parsing complete. Uploading GPU resources...";
    m_CompletedImport = std::move(result);
    m_Status = Status::AwaitingCommit;
}

void AssetImportPanel::Draw(void* nativeWindowHandle)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            const bool canImport = m_Status != Status::Parsing &&
                m_Status != Status::AwaitingCommit;
            if (ImGui::MenuItem("Import FBX...", nullptr, false, canImport))
                SelectModel(nativeWindowHandle);
            if (ImGui::MenuItem("Asset Import", nullptr, m_ShowWindow))
                m_ShowWindow = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (!m_ShowWindow)
        return;

    ImGui::SetNextWindowSize(ImVec2(430.0f, 250.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Import", &m_ShowWindow))
    {
        ImGui::End();
        return;
    }

    const bool busy = m_Status == Status::Parsing ||
        m_Status == Status::AwaitingCommit;
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Import FBX..."))
        SelectModel(nativeWindowHandle);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(GetStatusLabel());

    ImGui::SeparatorText("Selection");
    if (m_SourcePath.empty())
        ImGui::TextUnformatted("None");
    else
        ImGui::TextWrapped("%s", m_SourcePath.filename().u8string().c_str());
    ImGui::TextWrapped("%s", m_Message.c_str());

    if (m_Status == Status::Ready)
    {
        ImGui::SeparatorText("Imported Resources");
        ImGui::Text(
            "%zu sections  %zu materials  %zu textures",
            m_SectionCount,
            m_MaterialCount,
            m_TextureCount);
        ImGui::Text("%zu texture candidates discovered",
            m_TextureCandidateCount);
    }

    if (!m_Warnings.empty() && ImGui::CollapsingHeader("Warnings"))
    {
        for (const std::string& warning : m_Warnings)
            ImGui::BulletText("%s", warning.c_str());
    }

    ImGui::End();
}

std::optional<AssetImport::ModelImportResult>
AssetImportPanel::TakeCompletedImport()
{
    if (m_Status != Status::AwaitingCommit || !m_CompletedImport)
        return std::nullopt;

    std::optional<AssetImport::ModelImportResult> completed =
        std::move(m_CompletedImport);
    m_CompletedImport.reset();
    return completed;
}

void AssetImportPanel::ReportCommitSuccess(
    const AssetImport::ImportedModelData& model,
    std::size_t submittedSectionCount)
{
    m_Status = Status::Ready;
    m_Message = "Model is ready.";
    m_Warnings = model.warnings;
    m_SectionCount = submittedSectionCount;
    m_MaterialCount = model.materials.size();
    m_TextureCount = model.textures.size();
    m_TextureCandidateCount = model.textureCandidates.size();
}

void AssetImportPanel::ReportCommitFailure(std::string error)
{
    m_Status = Status::Error;
    m_Message = error.empty() ? "GPU resource upload failed." : std::move(error);
}

void AssetImportPanel::SelectModel(void* nativeWindowHandle)
{
    const std::optional<std::filesystem::path> selected =
        FileDialog::OpenModelFile(nativeWindowHandle);
    if (selected)
        BeginImport(*selected);
}

void AssetImportPanel::BeginImport(const std::filesystem::path& path)
{
    if (Lowercase(path.extension().string()) != ".fbx")
    {
        m_SourcePath = path;
        m_Status = Status::Error;
        m_Message = "This importer currently accepts FBX files only.";
        return;
    }

    m_SourcePath = path;
    m_Warnings.clear();
    m_SectionCount = 0;
    m_MaterialCount = 0;
    m_TextureCount = 0;
    m_TextureCandidateCount = 0;
    m_Message = "Parsing FBX data...";
    m_Status = Status::Parsing;
    try
    {
        m_ImportFuture = std::async(
            std::launch::async,
            [path]() { return AssetImport::FbxModelImporter::Import(path); });
    }
    catch (const std::exception& exception)
    {
        m_Status = Status::Error;
        m_Message = std::string("Unable to start model parsing: ") +
            exception.what();
    }
}

const char* AssetImportPanel::GetStatusLabel() const
{
    switch (m_Status)
    {
    case Status::Idle: return "Idle";
    case Status::Parsing: return "Parsing";
    case Status::AwaitingCommit: return "Uploading";
    case Status::Ready: return "Ready";
    case Status::Error: return "Error";
    }
    return "Unknown";
}
