// SPDX-License-Identifier: MIT
#pragma once

#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

class AssetImportPanel;
class Texture2D;

/// 编辑器拥有的视口矩形。屏幕坐标用于输入，像素尺寸用于 GPU 渲染目标。
struct EditorViewportRegion
{
    ImVec2 min{};
    ImVec2 size{};
    unsigned int pixelWidth = 0;
    unsigned int pixelHeight = 0;
    bool visible = false;
    bool hovered = false;
    bool focused = false;

    bool Contains(double x, double y) const
    {
        return visible && x >= min.x && y >= min.y &&
            x < min.x + size.x && y < min.y + size.y;
    }
};

/// 只负责 ImGui 工作区、面板布局与视口屏幕坐标，不拥有场景 GPU 目标。
class EditorWorkspace
{
public:
    EditorWorkspace();

    void BeginFrame(AssetImportPanel& assetImport, void* nativeWindowHandle);
    EditorViewportRegion BeginViewport();
    void DrawViewportImage(unsigned int textureId);
    void EndViewport();
    void DrawBottomPanels(AssetImportPanel& assetImport, void* nativeWindowHandle);
    void AddLog(std::string message);

    bool ShowMaterialEditor() const { return m_ShowMaterialEditor; }
    bool ShowStatistics() const { return m_ShowStatistics; }
    const EditorViewportRegion& GetViewportRegion() const { return m_ViewportRegion; }

private:
    void BuildDefaultLayout(ImGuiID dockspaceId);
    void RefreshAssets();
    void DrawDirectoryNode(const std::filesystem::path& path, int depth);
    void DrawContentBrowser(AssetImportPanel& assetImport, void* nativeWindowHandle);
    void DrawConsole();

    bool m_LayoutChecked = false;
    bool m_ResetLayout = false;
    bool m_SkipViewportFrame = false;
    bool m_ShowMaterialEditor = false;
    bool m_ShowStatistics = false;
    bool m_ViewportOpen = false;
    EditorViewportRegion m_ViewportRegion;
    std::filesystem::path m_AssetRoot;
    std::filesystem::path m_CurrentDirectory;
    std::optional<std::filesystem::path> m_PendingDirectory;
    std::vector<std::filesystem::directory_entry> m_Entries;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_Previews;
    std::deque<std::string> m_Log;
};
