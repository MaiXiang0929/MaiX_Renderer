// SPDX-License-Identifier: MIT
#include "EditorWorkspace.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <system_error>
#include <vector>

#include <imgui_internal.h>
#include <lodepng.h>

#include "Editor/AssetImportPanel.h"
#include "Renderer/Resources/Texture2D.h"

namespace
{
std::shared_ptr<Texture2D> LoadSmallPreview(const std::filesystem::path& path)
{
    std::vector<unsigned char> source;
    unsigned int width = 0;
    unsigned int height = 0;
    if (lodepng::decode(source, width, height, path.string()) != 0 ||
        width == 0 || height == 0)
        return nullptr;

    constexpr unsigned int PreviewSize = 64;
    std::vector<unsigned char> pixels(PreviewSize * PreviewSize * 4);
    for (unsigned int y = 0; y < PreviewSize; ++y)
    {
        for (unsigned int x = 0; x < PreviewSize; ++x)
        {
            const unsigned int sourceX = x * width / PreviewSize;
            const unsigned int sourceY = y * height / PreviewSize;
            const std::size_t from = 4 * (static_cast<std::size_t>(sourceY) * width + sourceX);
            const std::size_t to = 4 * (static_cast<std::size_t>(y) * PreviewSize + x);
            std::copy_n(source.data() + from, 4, pixels.data() + to);
        }
    }
    // 预览纹理只占 64² RGBA8，避免浏览高分辨率贴图时复制大量 GPU 资源。
    return Texture2D::CreateRGBA8(
        PreviewSize, PreviewSize, pixels, TextureColorSpace::Linear);
}
}

EditorWorkspace::EditorWorkspace()
    : m_AssetRoot(std::filesystem::current_path() / "assets")
    , m_CurrentDirectory(m_AssetRoot)
{
    RefreshAssets();
}

void EditorWorkspace::BuildDefaultLayout(ImGuiID dockspaceId)
{
    // DockBuilder 属于 ImGui 内部 API，集中在这里使用，避免面板依赖其实现细节。
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID left = dockspaceId;
    ImGuiID right = ImGui::DockBuilderSplitNode(left, ImGuiDir_Right, 0.32f, nullptr, &left);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.20f, nullptr, &left);
    ImGuiID scene = right;
    ImGuiID inspector = ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.50f, nullptr, &scene);

    ImGui::DockBuilderDockWindow("Viewport", left);
    // 两个工具窗口停靠到同一节点，显示为左下区域的两个标签。
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Content Browser", bottom);
    ImGui::DockBuilderDockWindow("Scene", scene);
    ImGui::DockBuilderDockWindow("Inspector", inspector);
    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorWorkspace::BeginFrame(AssetImportPanel& assetImport, void* nativeWindowHandle)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            assetImport.DrawFileMenuItems(nativeWindowHandle);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Material Editor", nullptr, &m_ShowMaterialEditor);
            ImGui::MenuItem("Renderer Statistics", nullptr, &m_ShowStatistics);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
                m_ResetLayout = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // 布局版本更新时更换稳定 ID，使旧版停靠配置在首次启动时重建；之后仍保存用户调整。
    const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(
        ImHashStr("MaiXEditorDockspaceV2"), ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);
    const ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspaceId);
    m_SkipViewportFrame = !m_LayoutChecked || m_ResetLayout;
    if (m_ResetLayout || (!m_LayoutChecked && (!root || !root->IsSplitNode())))
        BuildDefaultLayout(dockspaceId);
    m_LayoutChecked = true;
    m_ResetLayout = false;
}

EditorViewportRegion EditorWorkspace::BeginViewport()
{
    m_ViewportRegion = {};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    m_ViewportOpen = ImGui::Begin("Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (!m_ViewportOpen)
        return m_ViewportRegion;

    m_ViewportRegion.min = ImGui::GetCursorScreenPos();
    m_ViewportRegion.size = ImGui::GetContentRegionAvail();
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    // 首帧或重置布局后等待停靠矩形稳定，避免按整个窗口分配一次临时 GPU 目标。
    m_ViewportRegion.visible = !m_SkipViewportFrame &&
        m_ViewportRegion.size.x >= 64.0f &&
        m_ViewportRegion.size.y >= 64.0f;
    if (m_ViewportRegion.visible)
    {
        m_ViewportRegion.pixelWidth = static_cast<unsigned int>(
            std::max(1.0f, std::round(m_ViewportRegion.size.x * scale.x)));
        m_ViewportRegion.pixelHeight = static_cast<unsigned int>(
            std::max(1.0f, std::round(m_ViewportRegion.size.y * scale.y)));
    }
    m_ViewportRegion.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
        m_ViewportRegion.Contains(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
    m_ViewportRegion.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    return m_ViewportRegion;
}

void EditorWorkspace::DrawViewportImage(unsigned int textureId)
{
    if (m_ViewportOpen && m_ViewportRegion.visible && textureId != 0)
    {
        // OpenGL 附件原点在左下，ImGui 图像原点在左上，显示时翻转 V 坐标。
        ImGui::Image(static_cast<ImTextureID>(textureId), m_ViewportRegion.size,
            ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
    else if (m_ViewportOpen)
    {
        ImGui::TextUnformatted("Viewport is not ready.");
    }
}

void EditorWorkspace::EndViewport()
{
    ImGui::End();
    m_ViewportOpen = false;
}

void EditorWorkspace::RefreshAssets()
{
    m_Entries.clear();
    m_Previews.clear();
    std::error_code error;
    for (std::filesystem::directory_iterator it(m_CurrentDirectory, error), end;
         !error && it != end; it.increment(error))
        m_Entries.push_back(*it);
    std::sort(m_Entries.begin(), m_Entries.end(),
        [](const auto& lhs, const auto& rhs) {
            if (lhs.is_directory() != rhs.is_directory())
                return lhs.is_directory();
            return lhs.path().filename().string() < rhs.path().filename().string();
        });
}

void EditorWorkspace::DrawDirectoryNode(const std::filesystem::path& path, int depth)
{
    if (depth > 2)
        return;
    const std::string label = path.filename().string();
    const bool selected = path == m_CurrentDirectory;
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
        (selected ? ImGuiTreeNodeFlags_Selected : 0);
    const bool open = ImGui::TreeNodeEx(path.string().c_str(), flags, "%s", label.c_str());
    if (ImGui::IsItemClicked() && path != m_CurrentDirectory)
    {
        m_CurrentDirectory = path;
        RefreshAssets();
    }
    if (open)
    {
        std::error_code error;
        for (std::filesystem::directory_iterator it(path, error), end;
             !error && it != end; it.increment(error))
        {
            if (it->is_directory())
                DrawDirectoryNode(it->path(), depth + 1);
        }
        ImGui::TreePop();
    }
}

void EditorWorkspace::DrawContentBrowser(AssetImportPanel& assetImport, void* nativeWindowHandle)
{
    if (m_PendingDirectory)
    {
        m_CurrentDirectory = *m_PendingDirectory;
        m_PendingDirectory.reset();
        RefreshAssets();
    }
    if (!ImGui::Begin("Content Browser"))
    {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Import FBX"))
        assetImport.OpenImportDialog(nativeWindowHandle);
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        RefreshAssets();
    ImGui::Separator();
    ImGui::TextWrapped("%s", m_CurrentDirectory.string().c_str());

    const float treeWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x * 0.28f);
    ImGui::BeginChild("AssetTree", ImVec2(treeWidth, 0.0f), ImGuiChildFlags_Borders);
    DrawDirectoryNode(m_AssetRoot, 0);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("AssetList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

    const float tileWidth = 96.0f;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / tileWidth));
    std::optional<std::filesystem::path> requestedDirectory;
    if (ImGui::BeginTable("AssetTiles", columns))
    {
        for (const auto& entry : m_Entries)
        {
            ImGui::TableNextColumn();
            const std::string name = entry.path().filename().string();
            ImGui::PushID(name.c_str());
            if (entry.is_directory())
            {
                if (ImGui::Button("[Folder]", ImVec2(80.0f, 72.0f)))
                    requestedDirectory = entry.path();
            }
            else if (entry.path().extension() == ".png" && m_Previews.size() < 24)
            {
                const std::string key = entry.path().string();
                auto [previewIt, inserted] = m_Previews.try_emplace(key);
                if (inserted)
                    previewIt->second = LoadSmallPreview(entry.path());
                const auto& preview = previewIt->second;
                if (preview)
                    ImGui::Image(static_cast<ImTextureID>(preview->GetID()),
                        ImVec2(80.0f, 72.0f), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                else
                    ImGui::Button("[Image]", ImVec2(80.0f, 72.0f));
            }
            else
                ImGui::Button("[Asset]", ImVec2(80.0f, 72.0f));
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (requestedDirectory)
    {
        // 本帧 Image 绘制命令仍引用预览纹理，下一帧再释放旧目录的缓存。
        m_PendingDirectory = *requestedDirectory;
    }
    ImGui::EndChild();
    ImGui::End();
}

void EditorWorkspace::DrawConsole()
{
    if (!ImGui::Begin("Console"))
    {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Clear"))
        m_Log.clear();
    ImGui::Separator();
    ImGui::BeginChild("ConsoleLines", ImVec2(0.0f, 0.0f));
    for (const std::string& line : m_Log)
        ImGui::TextUnformatted(line.c_str());
    ImGui::EndChild();
    ImGui::End();
}

void EditorWorkspace::DrawBottomPanels(AssetImportPanel& assetImport, void* nativeWindowHandle)
{
    DrawConsole();
    DrawContentBrowser(assetImport, nativeWindowHandle);
}

void EditorWorkspace::AddLog(std::string message)
{
    constexpr std::size_t MaxLines = 128;
    if (m_Log.size() == MaxLines)
        m_Log.pop_front();
    m_Log.push_back(std::move(message));
}
