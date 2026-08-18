// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include "Assets/Import/ImportedModelData.h"

class AssetImportPanel
{
public:
    AssetImportPanel() = default;
    ~AssetImportPanel();

    AssetImportPanel(const AssetImportPanel&) = delete;
    AssetImportPanel& operator=(const AssetImportPanel&) = delete;

    // Polls CPU parsing without blocking. This does not call ImGui or OpenGL.
    void Update();
    void Draw(void* nativeWindowHandle = nullptr);

    std::optional<AssetImport::ModelImportResult> TakeCompletedImport();
    void ReportCommitSuccess(
        const AssetImport::ImportedModelData& model,
        std::size_t submittedSectionCount);
    void ReportCommitFailure(std::string error);

private:
    enum class Status
    {
        Idle,
        Parsing,
        AwaitingCommit,
        Ready,
        Error
    };

    void SelectModel(void* nativeWindowHandle);
    void BeginImport(const std::filesystem::path& path);
    const char* GetStatusLabel() const;

    Status m_Status = Status::Idle;
    bool m_ShowWindow = true;
    std::filesystem::path m_SourcePath;
    std::future<AssetImport::ModelImportResult> m_ImportFuture;
    std::optional<AssetImport::ModelImportResult> m_CompletedImport;
    std::string m_Message = "No model selected.";
    std::vector<std::string> m_Warnings;
    std::size_t m_SectionCount = 0;
    std::size_t m_MaterialCount = 0;
    std::size_t m_TextureCount = 0;
    std::size_t m_TextureCandidateCount = 0;
};
