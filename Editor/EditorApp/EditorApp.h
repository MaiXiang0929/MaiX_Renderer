// SPDX-License-Identifier: MIT
/// @file EditorApp.h
/// @brief 应用程序核心管理类的头文件
/// @details 该类负责管理整个程序的生命周期，包括初始化、主循环更新、渲染及资源清理。
/// @author MaiX
/// @date 2026-07-31

#pragma once

#include "Engine/Runtime/Engine/EngineSession.h"
#include "Engine/Core/Platform/WindowContext.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "cyVector.h"

#include "Engine/Runtime/Component/Camera.h"
#include "Engine/Runtime/Scene/SceneCamera.h"
#include "Engine/Runtime/Scene/SceneLight.h"
#include "Engine/Runtime/Scene/SceneModel.h"
#include "Editor/Session/EditorSelection.h"
#include "Editor/Session/EditorMaterialSelection.h"
#include "Engine/Rendering/Renderer/Resources/RenderResourceHandle.h"

struct GLFWwindow;
class Renderer;
class RendererStatisticsPanel;
class MaterialEditorPanel;
class AssetImportPanel;
class EditorViewportController;
class SceneHierarchyPanel;
class InspectorPanel;
class EditorWorkspace;
namespace AssetImport
{
struct ImportedModelData;
}

/// @brief 应用程序核心管理类
/// @details 该类负责管理整个程序的生命周期，包括初始化、主循环更新、渲染及资源清理。
class EditorApp
{

public:
	/// @brief 构造函数
    EditorApp(
        std::string normalMapPath,
        std::string displacementMapPath,
        std::uint32_t instanceGridSize = 0,
        bool materialLab = false,
        bool translucencyTest = false,
        std::string faceShadowDemoModelPath = {},
        std::string faceShadowDemoTexturePath = {},
        std::string faceShadowDemoMaterialName = {},
        bool materialInstanceLab = false);

	/// @brief 析构函数，自动调用 Shutdown 释放资源
    ~EditorApp();

	// 禁止复制
    EditorApp(const EditorApp&) = delete;
    EditorApp& operator=(const EditorApp&) = delete;

	/// @brief 运行应用程序
    void Run();

private:
    EngineSession m_Engine;
    WindowContext m_Context;
    GLFWwindow* m_Window        = nullptr;      ///< GLFW 窗口对象的指针句柄

    Renderer* m_Renderer        = nullptr;      ///< 从 EngineSession 借用；不拥有资源
    std::unique_ptr<RendererStatisticsPanel> m_StatisticsPanel;
    std::unique_ptr<MaterialEditorPanel> m_MaterialEditorPanel;
    std::unique_ptr<AssetImportPanel> m_AssetImportPanel;
    std::unique_ptr<EditorViewportController> m_ViewportController;
    std::unique_ptr<SceneHierarchyPanel> m_SceneHierarchyPanel;
    std::unique_ptr<InspectorPanel> m_InspectorPanel;
    std::unique_ptr<EditorWorkspace> m_Workspace;
    std::string m_ImGuiIniPath;
    bool m_ImGuiInitialized     = false;

	Camera m_Camera;                            ///< 编辑器场景视口所使用的摄像机

    bool m_SceneCameraView = false;
	std::string m_NormalMapPath;
    std::string m_DisplacementMapPath;
    std::uint32_t m_InstanceGridSize = 0;
    bool m_MaterialLab = false;
    bool m_MaterialInstanceLab = false;
    bool m_TranslucencyTest = false;
    std::string m_FaceShadowDemoModelPath;
    std::string m_FaceShadowDemoTexturePath;
    std::string m_FaceShadowDemoMaterialName;

    bool m_Initialized          = false;        ///< 应用程序是否已初始化

    unsigned int m_Width        = 1920;         ///< 窗口宽度
    unsigned int m_Height       = 1080;         ///< 窗口高度


    EditorSelection m_EditorSelection;
    // IDs survive model-vector reallocation; resolved once when the viewport is valid.
    EditorSelection m_PendingFocusSelection;
    EditorMaterialSelection m_MaterialSelection;

private:
    /// @brief 初始化应用程序
    /// @return 初始化成功返回 true，否则返回 false
    bool Init();
    bool CreateStartupScene(const struct StartupSceneDefinition& scene);
    bool CreateBasicStartupScene(const struct StartupSceneDefinition& scene);

    /// @brief 创建用于验收透明排序、混合和深度遮挡的最小场景。
    void CreateTranslucencyTestScene();

    bool CommitImportedModel(
        AssetImport::ImportedModelData& model,
        std::string& error,
        ModelId& importedId);
    bool LoadStartupFaceShadowDemo();
    void DestroyModelResources(SceneModel& model);
    bool RemoveModel(ModelId id);
    void ApplyMaterialEditRequest(const struct MaterialEditRequest& request);
    SceneLight* FindSceneLight(std::uint32_t id);
    const SceneLight* FindSceneLight(std::uint32_t id) const;
    
	/// @brief 更新应用程序状态
    void Update();
    
	/// @brief 渲染应用程序
    void Render();
    
	/// @brief 关闭应用程序
    void Shutdown();

    // 回调函数必须是 static 的，配合 UserPointer 使用
    static void FramebufferSizeCallback(
        GLFWwindow* window,
        int width,
        int height);

    static void MouseButtonCallback(GLFWwindow* window,
        int button,
        int action,
        int mods);

    static void CursorPositionCallback(GLFWwindow* window,
        double xpos,
        double ypos);

    static void ScrollCallback(GLFWwindow* window,
        double xoffset,
        double yoffset);

    static void CharCallback(GLFWwindow* window,
        unsigned int codepoint);

    static void KeyCallback(GLFWwindow* window,
        int key,
        int scancode,
        int action,
        int mods);   
};
