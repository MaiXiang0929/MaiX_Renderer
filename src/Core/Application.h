// SPDX-License-Identifier: MIT
/// @file Application.h
/// @brief 应用程序核心管理类的头文件
/// @details 该类负责管理整个程序的生命周期，包括初始化、主循环更新、渲染及资源清理。
/// @author MaiX
/// @date 2026-07-31

#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "cyVector.h"

#include "Camera.h"
#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"
#include "Editor/EditorSelection.h"
#include "Renderer/Resources/RenderResourceHandle.h"

struct GLFWwindow;
class Renderer;
class RendererStatisticsPanel;
class MaterialEditorPanel;
class AssetImportPanel;
class EditorViewportController;
class SceneHierarchyPanel;
class InspectorPanel;
namespace AssetImport
{
struct ImportedModelData;
}

/// @brief 应用程序核心管理类
/// @details 该类负责管理整个程序的生命周期，包括初始化、主循环更新、渲染及资源清理。
class Application
{

public:
	/// @brief 构造函数
    Application(
        std::string normalMapPath,
        std::string displacementMapPath,
        std::uint32_t instanceGridSize = 0,
        bool materialLab = false,
        bool translucencyTest = false,
        std::string faceShadowDemoModelPath = {},
        std::string faceShadowDemoTexturePath = {},
        std::string faceShadowDemoMaterialName = {});

	/// @brief 析构函数，自动调用 Shutdown 释放资源
    ~Application();

	// 禁止复制
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

	/// @brief 运行应用程序
    void Run();

private:
    GLFWwindow* m_Window        = nullptr;      ///< GLFW 窗口对象的指针句柄

    Renderer* m_Renderer        = nullptr;      ///< 渲染器对象的指针句柄
    std::unique_ptr<RendererStatisticsPanel> m_StatisticsPanel;
    std::unique_ptr<MaterialEditorPanel> m_MaterialEditorPanel;
    std::unique_ptr<AssetImportPanel> m_AssetImportPanel;
    std::unique_ptr<EditorViewportController> m_ViewportController;
    std::unique_ptr<SceneHierarchyPanel> m_SceneHierarchyPanel;
    std::unique_ptr<InspectorPanel> m_InspectorPanel;
    bool m_ImGuiInitialized     = false;

	Camera m_Camera;                            ///< 编辑器场景视口所使用的摄像机

	std::string m_ObjPath = "assets/models/teapot.obj";
	std::string m_NormalMapPath;
    std::string m_DisplacementMapPath;
    std::uint32_t m_InstanceGridSize = 0;
    bool m_MaterialLab = false;
    bool m_TranslucencyTest = false;
    std::string m_FaceShadowDemoModelPath;
    std::string m_FaceShadowDemoTexturePath;
    std::string m_FaceShadowDemoMaterialName;
    float m_SceneRadius = 1.0f;

    bool m_Initialized          = false;        ///< 应用程序是否已初始化

    unsigned int m_Width        = 1920;         ///< 窗口宽度
    unsigned int m_Height       = 1080;         ///< 窗口高度

    cy::Vec3f m_ObjCenter;                     ///< 模型包围盒中心
    float m_GroundY         = 0.0f;            ///< 反射地面 Y 坐标（模型包围盒底部）
    float m_ModelDiameter   = 0.0f;            ///< 模型包围盒直径，用于缩放地面
    std::uint32_t m_MainLightId = std::numeric_limits<std::uint32_t>::max();

    struct ModelResourceGroup
    {
        std::vector<std::uint32_t> primitives;
        std::vector<MeshHandle> meshes;
        std::vector<MaterialHandle> materials;
    };
    ModelResourceGroup m_ActiveModelResources;
    EditorSelection m_EditorSelection;
    EditableModel m_ActiveModel;
    std::vector<EditableLight> m_EditableLights;

private:
    /// @brief 初始化应用程序
    /// @return 初始化成功返回 true，否则返回 false
    bool Init();

    /// @brief 创建用于验收透明排序、混合和深度遮挡的最小场景。
    void CreateTranslucencyTestScene();

    bool CommitImportedModel(
        AssetImport::ImportedModelData& model,
        std::string& error);
    bool LoadStartupFaceShadowDemo();
    void DestroyModelResources(ModelResourceGroup& resources);
    EditableLight* FindEditableLight(std::uint32_t id);
    const EditableLight* FindEditableLight(std::uint32_t id) const;
    
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
