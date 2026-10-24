#include "Engine/Rendering/RHI/OpenGL/OpenGLNativeBridge.h"
// SPDX-License-Identifier: MIT
#include "Editor/Session/EditorWorkspace.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <system_error>
#include <vector>
#include <fstream>
#include <iterator>

#include <imgui_internal.h>
#include <lodepng.h>

#include "Editor/ContentBrowser/AssetImportPanel.h"
#include "Editor/Session/EditorTheme.h"
#include "Editor/Session/EditorWidgets.h"
#include "Editor/Session/EditorProject.h"
#include "Engine/Core/Platform/Windows/FileDialog.h"
#include <cctype>
#include <cfloat>
#include "Engine/Rendering/Renderer/Resources/Texture2D.h"

namespace
{
constexpr ImGuiWindowFlags StatusBarFlags = ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavFocus;

std::shared_ptr<Texture2D> LoadSmallPreview(RHIDevice& device, const std::filesystem::path& path)
{
    std::vector<unsigned char> source;
    std::ifstream input(path, std::ios::binary);
    if (!input) return nullptr;
    const std::vector<unsigned char> encoded((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    unsigned int width = 0;
    unsigned int height = 0;
    if (lodepng::decode(source, width, height, encoded) != 0 ||
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
    return Texture2D::CreateRGBA8(device,
        PreviewSize, PreviewSize, pixels, TextureColorSpace::Linear);
}
}

EditorWorkspace::EditorWorkspace(RHIDevice& device, EditorProject& project)
    : m_Device(device)
    , m_Project(project)
    , m_AssetRoot(project.AssetsRoot())
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
    constexpr float inspectorRatio = 0.305f;
    ImGuiID inspector = ImGui::DockBuilderSplitNode(left, ImGuiDir_Right, inspectorRatio, nullptr, &left);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.42f, nullptr, &left);
    constexpr float sceneRatio = 0.165f / (1.0f-inspectorRatio);
    ImGuiID scene = ImGui::DockBuilderSplitNode(left, ImGuiDir_Left, sceneRatio, nullptr, &left);

    ImGui::DockBuilderDockWindow("Viewport", left);
    // 两个工具窗口停靠到同一节点，显示为左下区域的两个标签。
    ImGui::DockBuilderDockWindow("Content Browser", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Scene", scene);
    ImGui::DockBuilderDockWindow("Inspector", inspector);
    // 共用现有 Inspector 节点，不新增分区，也不改变其他面板的比例。
    ImGui::DockBuilderDockWindow("Material Editor", inspector);
    ImGui::DockBuilderDockWindow("Render Settings", inspector);
    ImGui::DockBuilderFinish(dockspaceId);
    m_MaterialDockChecked = false;
    m_ContentFocusRequested = true;
}

void EditorWorkspace::OpenMaterialEditor()
{
    m_ShowMaterialEditor = true;
    m_MaterialFocusRequested = true;
}

void EditorWorkspace::PrepareMaterialEditor()
{
    if (!m_ShowMaterialEditor)
        return;

    if (!m_MaterialDockChecked)
    {
        const ImGuiWindow* inspector = ImGui::FindWindowByName("Inspector");
        if (inspector && inspector->DockId != 0 &&
            ImGui::DockBuilderGetNode(inspector->DockId))
        {
            // 旧配置中的浮动材质窗口只迁移自身；Inspector 已保存的节点和尺寸继续使用。
            ImGui::SetNextWindowDockID(inspector->DockId, ImGuiCond_Always);
            m_MaterialDockChecked = true;
        }
    }
    if (m_MaterialFocusRequested)
    {
        // 仅响应打开请求激活标签，不能每帧聚焦，否则会抢走视口输入。
        ImGui::SetNextWindowFocus();
        m_MaterialFocusRequested = false;
    }
}

void EditorWorkspace::SynchronizeProject()
{
    if (m_AssetRoot != m_Project.AssetsRoot())
    {
        m_AssetRoot = m_Project.AssetsRoot();
        m_CurrentDirectory = m_AssetRoot;
        m_PendingDirectory.reset();
        m_AssetFilter[0] = '\0';
        RefreshAssets();
    }
}

void EditorWorkspace::BeginFrame(AssetImportPanel& assetImport, void* nativeWindowHandle, EditorAppearanceSettings& appearance)
{
    SynchronizeProject();
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (m_SceneFileMenu) { m_SceneFileMenu(); ImGui::Separator(); }
            assetImport.DrawFileMenuItems(nativeWindowHandle);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::BeginMenu("Theme"))
            {
                if (ImGui::MenuItem("Light",nullptr,appearance.theme==EditorThemeMode::Light))
                { appearance.theme=EditorThemeMode::Light; ImGui::MarkIniSettingsDirty(); }
                if (ImGui::MenuItem("Dark",nullptr,appearance.theme==EditorThemeMode::Dark))
                { appearance.theme=EditorThemeMode::Dark; ImGui::MarkIniSettingsDirty(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("UI Scale"))
            {
                const float scales[] = {1.0f,1.25f,1.5f,2.0f};
                const char* labels[] = {"100%","125%","150%","200%"};
                for (int index=0; index<4; ++index)
                    if (ImGui::MenuItem(labels[index],nullptr,appearance.uiScale==scales[index]))
                    { appearance.uiScale=scales[index]; ImGui::MarkIniSettingsDirty(); }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Material Editor", nullptr, &m_ShowMaterialEditor) &&
                m_ShowMaterialEditor)
                OpenMaterialEditor();
            ImGui::MenuItem("Renderer Statistics", nullptr, &m_ShowStatistics);
            ImGui::MenuItem("Render Settings", nullptr, &m_ShowRenderSettings);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
                m_ResetLayout = true;
            ImGui::EndMenu();
        }
        const float projectX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x -
            ImGui::CalcTextSize(m_Project.Name().c_str()).x;
        if (projectX > ImGui::GetCursorPosX()+20*EditorTheme::GetScale())
        {
            ImGui::SetCursorPosX(projectX);
            ImGui::TextDisabled("%s",m_Project.Name().c_str());
        }
        ImGui::EndMainMenuBar();
    }

    // Register next-frame work insets before docking; draw selection after panel edits.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        ImVec2(8*EditorTheme::GetScale(),3*EditorTheme::GetScale()));
    ImGui::BeginViewportSideBar("##MaiXStatusBar",ImGui::GetMainViewport(),ImGuiDir_Down,
        ImGui::GetFrameHeight(),StatusBarFlags);
    ImGui::End();
    ImGui::PopStyleVar();

    // Keep the stable ID: existing user docking is retained; Reset Layout applies new defaults.
    const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(
        ImHashStr("MaiXEditorDockspaceV3"), ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);
    const ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspaceId);
    m_SkipViewportFrame = !m_LayoutChecked || m_ResetLayout;
    if (m_ResetLayout || (!m_LayoutChecked && (!root || !root->IsSplitNode())))
        BuildDefaultLayout(dockspaceId);
    m_LayoutChecked = true;
    m_ResetLayout = false;
}

void EditorWorkspace::DrawStatusBar(const char* selectionLabel)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        ImVec2(8*EditorTheme::GetScale(),3*EditorTheme::GetScale()));
    if (ImGui::Begin("##MaiXStatusBar",nullptr,StatusBarFlags))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(4*EditorTheme::GetScale(),0));
        if (ImGui::BeginTable("##Status",2))
        {
            const float projectWidth = std::min(ImGui::CalcTextSize(m_Project.Name().c_str()).x,
                ImGui::GetContentRegionAvail().x*.4f);
            ImGui::TableSetupColumn("Selection",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Project",ImGuiTableColumnFlags_WidthFixed,projectWidth);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(selectionLabel);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",selectionLabel);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s",m_Project.Name().c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",m_Project.Root().u8string().c_str());
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

EditorViewportRegion EditorWorkspace::BeginViewport(const std::function<void()>& drawToolbar)
{
    m_ViewportRegion = {};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    m_ViewportOpen = ImGui::Begin("Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (!m_ViewportOpen)
        return m_ViewportRegion;

    const float inset = 7*EditorTheme::GetScale();
    ImGui::SetCursorPos(ImVec2(inset,ImGui::GetCursorPosY()+inset));
    drawToolbar();
    ImGui::Separator();
    // Only the image rectangle is used for picking, navigation and target sizing.
    ImGui::SetCursorPosX(0);
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
    m_BrowserError.clear();
    if (!m_Project.IsReady()) return;
    if (!m_Project.ContainsAssetPath(m_CurrentDirectory))
    {
        m_BrowserError = "This folder is outside the current project's Assets.";
        return;
    }
    std::error_code error;
    for (std::filesystem::directory_iterator it(m_CurrentDirectory, error), end;
         !error && it != end; it.increment(error))
    {
        if (m_Project.ContainsAssetPath(it->path())) m_Entries.push_back(*it);
    }
    if (error) m_BrowserError = "Cannot read folder: " + error.message();
    std::sort(m_Entries.begin(), m_Entries.end(),
        [](const auto& lhs, const auto& rhs) {
            std::error_code leftError, rightError;
            const bool leftDirectory = lhs.is_directory(leftError);
            const bool rightDirectory = rhs.is_directory(rightError);
            if (leftDirectory != rightDirectory) return leftDirectory;
            return lhs.path().filename().native() < rhs.path().filename().native();
        });
}

void EditorWorkspace::DrawDirectoryNode(const std::filesystem::path& path, int depth)
{
    if (depth > 2 || !m_Project.ContainsAssetPath(path))
        return;
    const std::string label = path.filename().u8string();
    const bool selected = path == m_CurrentDirectory;
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
        (depth == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0) |
        (selected ? ImGuiTreeNodeFlags_Selected : 0);
    const bool open = ImGui::TreeNodeEx(path.u8string().c_str(), flags, "");
    const auto itemMin=ImGui::GetItemRectMin();
    const float iconX=itemMin.x+ImGui::GetTreeNodeToLabelSpacing();
    const float scale=EditorTheme::GetScale();
    EditorWidgets::DrawIcon(EditorWidgets::Icon::Folder,ImVec2(iconX,itemMin.y+3*scale),16*scale);
    auto* draw=ImGui::GetWindowDrawList();
    draw->PushClipRect(itemMin,ImGui::GetItemRectMax(),true);
    draw->AddText(ImVec2(iconX+22*scale,itemMin.y+3*scale),ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
    draw->PopClipRect();
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
            std::error_code statusError;
            if (it->is_directory(statusError) && !statusError)
                DrawDirectoryNode(it->path(), depth + 1);
        }
        ImGui::TreePop();
    }
}

void EditorWorkspace::DrawContentBrowser(AssetImportPanel& assetImport)
{
    if (m_PendingDirectory)
    {
        const auto requested = *m_PendingDirectory;
        m_PendingDirectory.reset();
        if (m_Project.ContainsAssetPath(requested))
        {
            m_CurrentDirectory = requested;
            RefreshAssets();
        }
    }
    if (m_ContentFocusRequested)
    {
        ImGui::SetNextWindowFocus();
        m_ContentFocusRequested=false;
    }
    if (!ImGui::Begin("Content Browser"))
    {
        ImGui::End();
        return;
    }
    if (!m_Project.Diagnostic().empty())
        ImGui::TextWrapped("%s", m_Project.Diagnostic().c_str());
    if (!m_Project.IsReady())
    {
        ImGui::TextWrapped("Project Assets is unavailable. Open a project folder to continue browsing.");
        ImGui::End();
        return;
    }
    std::error_code relativeError;
    const auto relative = std::filesystem::relative(m_CurrentDirectory,m_AssetRoot,relativeError);
    std::string location = m_Project.Name() + " / Assets";
    if (!relativeError && relative != ".") location += " / " + relative.generic_u8string();
    const float scale = EditorTheme::GetScale();
    const float available = ImGui::GetContentRegionAvail().x;
    const bool inlineFilter = available >= 360*scale;
    if (ImGui::BeginTable("##BrowserToolbar",inlineFilter ? 2 : 1))
    {
        ImGui::TableSetupColumn("Path",ImGuiTableColumnFlags_WidthStretch);
        if (inlineFilter)
            ImGui::TableSetupColumn("Filter",ImGuiTableColumnFlags_WidthFixed,std::min(200*scale,available*.35f));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(location.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",m_CurrentDirectory.u8string().c_str());
        if (!inlineFilter) ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##AssetFilter","Filter current folder...",m_AssetFilter,sizeof(m_AssetFilter));
        ImGui::EndTable();
    }
    if (!m_BrowserError.empty()) ImGui::TextWrapped("%s", m_BrowserError.c_str());
    ImGui::Separator();

    const float treeWidth = std::min(200*scale,std::max(85*scale,ImGui::GetContentRegionAvail().x*.23f));
    ImGui::BeginChild("AssetTree", ImVec2(treeWidth, 0.0f));
    DrawDirectoryNode(m_AssetRoot, 0);
    ImGui::EndChild();
    const auto treeMax = ImGui::GetItemRectMax();
    const auto treeMin = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(treeMax.x+3*scale,treeMin.y),
        ImVec2(treeMax.x+3*scale,treeMax.y),ImGui::GetColorU32(ImGuiCol_Separator));
    ImGui::SameLine();
    ImGui::BeginChild("AssetList", ImVec2(0.0f, 0.0f));

    const float tileWidth = 88.0f*scale;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / tileWidth));
    std::optional<std::filesystem::path> requestedDirectory;
    const auto drawTile = [](EditorWidgets::Icon icon, ImTextureID preview = 0) {
        const float scale = EditorTheme::GetScale();
        const bool pressed = ImGui::InvisibleButton("##Asset", ImVec2(72*scale,48*scale));
        const auto p = ImGui::GetItemRectMin();
        auto* draw = ImGui::GetWindowDrawList();
        if (ImGui::IsItemHovered())
            draw->AddRectFilled(p,ImGui::GetItemRectMax(),ImGui::GetColorU32(ImGuiCol_HeaderHovered),4*scale);
        const ImVec2 origin(p.x+18*scale,p.y+6*scale);
        if (preview)
            draw->AddImage(preview,origin,ImVec2(origin.x+36*scale,origin.y+36*scale),ImVec2(0,1),ImVec2(1,0));
        else
            EditorWidgets::DrawIcon(icon,origin,36*scale);
        return pressed;
    };
    if (ImGui::BeginTable("AssetTiles", columns))
    {
        for (const auto& entry : m_Entries)
        {
            if (!m_Project.ContainsAssetPath(entry.path())) continue;
            const std::string name = entry.path().filename().u8string();
            std::string foldedName=name, foldedFilter=m_AssetFilter;
            const auto fold=[](unsigned char c) { return static_cast<char>(std::tolower(c)); };
            std::transform(foldedName.begin(),foldedName.end(),foldedName.begin(),fold);
            std::transform(foldedFilter.begin(),foldedFilter.end(),foldedFilter.begin(),fold);
            if (!foldedFilter.empty() && foldedName.find(foldedFilter)==std::string::npos) continue;
            ImGui::TableNextColumn();
            ImGui::PushID(name.c_str());
            std::error_code statusError;
            const bool directory = entry.is_directory(statusError);
            if (statusError) { ImGui::PopID(); continue; }
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), fold);
            if (directory)
            {
                if (drawTile(EditorWidgets::Icon::Folder))
                    requestedDirectory = entry.path();
            }
            else if (extension == ".png" &&
                (m_Previews.count(entry.path().u8string()) != 0 || m_Previews.size() < 24))
            {
                const std::string key = entry.path().u8string();
                auto [previewIt, inserted] = m_Previews.try_emplace(key);
                if (inserted)
                    previewIt->second = LoadSmallPreview(m_Device,entry.path());
                const auto& preview = previewIt->second;
                drawTile(EditorWidgets::Icon::File, preview ?
                    static_cast<ImTextureID>(GetOpenGLTexture(m_Device,preview->GetID())) : 0);
            }
            else
                drawTile(EditorWidgets::Icon::File);
            if (!directory && ImGui::IsItemHovered())
            {
                if (extension == ".fbx")
                {
                    ImGui::SetTooltip("Double-click to load this FBX into the current scene.");
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        assetImport.BeginImport(entry.path());
                }
                else ImGui::SetTooltip("%s", entry.path().u8string().c_str());
            }
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (m_Entries.empty() && m_BrowserError.empty())
        ImGui::TextWrapped("Assets is empty. Place your project files in this folder to browse them here.");
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

void EditorWorkspace::DrawBottomPanels(AssetImportPanel& assetImport)
{
    DrawConsole();
    DrawContentBrowser(assetImport);
}

void EditorWorkspace::AddLog(std::string message)
{
    constexpr std::size_t MaxLines = 128;
    if (m_Log.size() == MaxLines)
        m_Log.pop_front();
    m_Log.push_back(std::move(message));
}
