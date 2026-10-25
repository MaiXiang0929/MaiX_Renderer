// SPDX-License-Identifier: MIT
/// @file EditorApp.cpp
/// @brief 应用程序核心管理类的实现文件
/// @details 该文件实现了 EditorApp 类的核心功能，包括初始化、主循环更新、渲染及资源清理。
/// @author MaiX
/// @date 2026-07-31

#include"Editor/EditorApp/EditorApp.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Editor/Diagnostics/RendererStatisticsPanel.h"
#include "Editor/Inspector/RenderSettingsPanel.h"
#include "Editor/Inspector/EnvironmentPanel.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Rendering/RHI/OpenGL/OpenGLNativeBridge.h"
#include "Engine/Runtime/Scene/InstanceGrid.h"
#include "Engine/Runtime/Engine/SceneFrameBuilder.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "Editor/ContentBrowser/AssetImportPanel.h"
#include "Engine/Asset/Importer/FbxModelImporter.h"
#include "Engine/Asset/Importer/ImageLoader.h"
#include "Editor/Viewport/EditorViewportController.h"
#include "Editor/Inspector/InspectorPanel.h"
#include "Editor/Inspector/ShadowSettingsPanel.h"
#include "Editor/MaterialEditor/MaterialEditorPanel.h"
#include "Editor/SceneHierarchy/SceneHierarchyPanel.h"
#include "Editor/Session/EditorWorkspace.h"
#include "Editor/Session/EditorFonts.h"
#include "Editor/Session/EditorTheme.h"
#include "Editor/Session/EditorWidgets.h"
#include "Editor/Session/EditorProject.h"
#include "Engine/Core/Platform/Windows/PlatformPaths.h"

#include "Engine/Asset/Geometry/CubeGeometry.h"
#include "Engine/Runtime/Scene/StartupScene.h"
#include "Engine/Rendering/Renderer/View/DirectionalShadow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
std::shared_ptr<Texture2D> CreateOrmCheckerTexture(
    RHIDevice& device,
    unsigned char roughnessLow,
    unsigned char roughnessHigh,
    unsigned char metallic)
{
    const std::vector<unsigned char> pixels = {
        255, roughnessLow,  metallic, 255,
        210, roughnessHigh, metallic, 255,
        210, roughnessHigh, metallic, 255,
        255, roughnessLow,  metallic, 255
    };
    return Texture2D::CreateRGBA8(device,
        2, 2, pixels, TextureColorSpace::Linear);
}

void* GetNativeWindowHandle(GLFWwindow* window)
{
#if defined(_WIN32)
    return static_cast<void*>(glfwGetWin32Window(window));
#else
    (void)window;
    return nullptr;
#endif
}
}

/// @brief 构造函数
EditorApp::EditorApp(
    std::string normalMapPath,
    std::string displacementMapPath,
    std::uint32_t instanceGridSize,
    bool materialLab,
    bool translucencyTest,
    std::string faceShadowDemoModelPath,
    std::string faceShadowDemoTexturePath,
    std::string faceShadowDemoMaterialName,
    bool materialInstanceLab)
    :
    m_Camera(
        cy::Vec3f(0, 0, 0),
        50.0f
    ),
    m_NormalMapPath(std::move(normalMapPath)),
    m_DisplacementMapPath(std::move(displacementMapPath)),
    m_InstanceGridSize(instanceGridSize),
    m_MaterialLab(materialLab),
    m_MaterialInstanceLab(materialInstanceLab),
    m_TranslucencyTest(translucencyTest),
    m_FaceShadowDemoModelPath(std::move(faceShadowDemoModelPath)),
    m_FaceShadowDemoTexturePath(std::move(faceShadowDemoTexturePath)),
    m_FaceShadowDemoMaterialName(std::move(faceShadowDemoMaterialName))
{

}

/// @brief 析构函数，自动调用 Shutdown 释放资源
EditorApp::~EditorApp() {
    Shutdown();
}

/// @brief 运行应用程序
void EditorApp::Run() {
    // 初始化引擎，如果失败直接退出
    bool initialized = false;
    try { initialized = Init(); }
    catch (const std::exception& error) { std::cerr << "[Error] " << error.what() << std::endl; }
    if (!initialized) {
		Shutdown();
        std::cerr << "[Error] Engine initialization failed!" << std::endl;
        return;
    }

    // 游戏主循环 (Game Loop)
    while (!glfwWindowShouldClose(m_Window)) {
        // 处理系统的底层事件（如键盘、鼠标输入触发）
        glfwPollEvents();

        // 执行逻辑更新与渲染
        Update();
        Render();

        // 交换前后缓冲区，把画面显示到屏幕上
        glfwSwapBuffers(m_Window);
    }
}

/// @brief 初始化应用程序
bool EditorApp::Init() {
    std::string contextError;
    if (!m_Context.Initialize(m_Width, m_Height, "MaiX Engine", contextError))
    {
        std::cerr << "[Error] " << contextError << std::endl;
        return false;
    }
    m_Window = m_Context.GetWindow(); // borrowed from the platform host

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth > 0 && framebufferHeight > 0) {
        m_Width = static_cast<unsigned int>(framebufferWidth);
        m_Height = static_cast<unsigned int>(framebufferHeight);
    }

    // 将当前的 EditorApp 实例指针绑定到 GLFW 窗口上
    glfwSetWindowUserPointer(m_Window, this);

    // 绑定所有的输入回调
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_Window, CursorPositionCallback);
    glfwSetScrollCallback(m_Window, ScrollCallback);
    glfwSetKeyCallback(m_Window, KeyCallback);
    glfwSetCharCallback(m_Window, CharCallback);

    // OpenGL 上下文已经可用，后续任一步失败都应由 Shutdown 统一释放资源。
    m_Initialized = true;

    // EditorApp 保留 GLFW 回调所有权，ImGui 后端只接收手动转发的输入事件。
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // 将布局保存在用户目录，避免 Visual Studio 的工作目录改变时丢失停靠状态。
#if defined(_WIN32)
    char* localAppData = nullptr;
    std::size_t localAppDataLength = 0;
    const bool hasLocalAppData =
        _dupenv_s(&localAppData, &localAppDataLength, "LOCALAPPDATA") == 0 &&
        localAppData != nullptr;
#else
    const char* localAppData = std::getenv("LOCALAPPDATA");
    const bool hasLocalAppData = localAppData != nullptr;
#endif
    if (hasLocalAppData)
    {
        const std::filesystem::path settingsDirectory =
            std::filesystem::path(localAppData) / "MaiX_Renderer";
        std::error_code error;
        std::filesystem::create_directories(settingsDirectory, error);
        if (!error)
        {
            m_ImGuiIniPath = (settingsDirectory / "imgui.ini").string();
            ImGui::GetIO().IniFilename = m_ImGuiIniPath.c_str();
        }
    }
#if defined(_WIN32)
    std::free(localAppData);
#endif
    const auto documents = PlatformPaths::DocumentsDirectory();
    const auto defaultProject = documents.empty() ? std::filesystem::path{} :
        documents / "MaiX Projects" / "DefaultProject";
    const std::filesystem::path engineSource(MAIX_ENGINE_SOURCE_DIR);
    m_Project = std::make_unique<EditorProject>(defaultProject,
        std::vector<std::filesystem::path>{engineSource / "assets", engineSource / "Shaders",
            engineSource / "out", PlatformPaths::ExecutableDirectory()});
    m_Project->RegisterSettings();
    EditorTheme::RegisterSettings(m_Appearance);
    // Load before applying the first style, avoiding a flash of the wrong theme.
    if (!m_ImGuiIniPath.empty())
        ImGui::LoadIniSettingsFromDisk(m_ImGuiIniPath.c_str());
    if (m_Project->Initialize())
        std::cout << "[Project] " << (m_Project->StartupProjectFile().empty()?"Assets: ":"Pending project: ") <<
            (m_Project->StartupProjectFile().empty()?m_Project->AssetsRoot():m_Project->StartupProjectFile()).u8string() << std::endl;
    else
        std::cerr << "[Project] " << m_Project->Diagnostic() << std::endl;
    const bool imguiPlatformInitialized =
        ImGui_ImplGlfw_InitForOpenGL(m_Window, false);
    const bool imguiRendererInitialized =
        imguiPlatformInitialized && ImGui_ImplOpenGL3_Init("#version 400");
    if (!imguiRendererInitialized)
    {
        if (imguiPlatformInitialized)
            ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        std::cerr << "[Error] Failed to initialize ImGui backends." << std::endl;
        return false;
    }
    m_ImGuiInitialized = true;
    std::string fontDiagnostic;
    float contentScaleX = 1.0f, contentScaleY = 1.0f;
    glfwGetWindowContentScale(m_Window, &contentScaleX, &contentScaleY);
    m_AppliedUiScale = std::clamp(contentScaleX * m_Appearance.uiScale, 0.75f, 4.0f);
    m_AppliedTheme = m_Appearance.theme;
    EditorTheme::Apply(m_AppliedTheme, m_AppliedUiScale);
    if (!EditorFonts::Initialize(*ImGui::GetIO().Fonts,
            "assets/fonts/NotoSansSC.ttf", fontDiagnostic, 15.0f * m_AppliedUiScale))
        std::cerr << "[Warning] " << fontDiagnostic << std::endl;
    else
        std::cout << "[Editor] Chinese font atlas initialized." << std::endl;
    m_StatisticsPanel = std::make_unique<RendererStatisticsPanel>();
    m_MaterialEditorPanel = std::make_unique<MaterialEditorPanel>();
    m_AssetImportPanel = std::make_unique<AssetImportPanel>();
    m_ViewportController = std::make_unique<EditorViewportController>();
    m_SceneHierarchyPanel = std::make_unique<SceneHierarchyPanel>();
    m_InspectorPanel = std::make_unique<InspectorPanel>();


    // 主颜色目标和显示平面都跟随窗口 framebuffer 的像素宽高比。
    const float framebufferAspect =
        static_cast<float>(m_Width) / static_cast<float>(m_Height);
    m_Camera.SetAspectRatio(framebufferAspect);

    std::cout << "[System] Engine Initialized Successfully! " << std::endl;

    // =======================================
	// 打印初始化信息（后续再加入完整 Debug Layer）
    // =======================================
    std::cout << "\n========== Renderer Info ================\n";

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;

    // OpenGL Capability
    int maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    std::cout << "Max Texture Size: " << maxTextureSize << std::endl;

    std::cout << "=========================================\n\n";

    // 开启 OpenGL 默认状态
	if (!m_Engine.Initialize()) {
        std::cerr << "[Error] Renderer initialization failed." << std::endl;
        return false;
    }
    m_Renderer = m_Engine.GetRenderer();
    m_Workspace = std::make_unique<EditorWorkspace>(m_Renderer->Device(), *m_Project);
    m_Workspace->SetSceneFileMenu([this] { DrawSceneFileMenu(); });
    m_Workspace->AddLog("MaiX Renderer initialized.");
    if (!m_Project->Diagnostic().empty())
        m_Workspace->AddLog("[Project] " + m_Project->Diagnostic());
    if (!fontDiagnostic.empty())
        m_Workspace->AddLog("[Warning] " + fontDiagnostic);
    std::string diagnostic;
    const auto scene = LoadStartupScene("assets/scenes/startup.json", diagnostic);
    std::cout << "[StartupScene] " << diagnostic << std::endl;
    if (!CreateStartupScene(scene)) return false;
    if (!m_Project->StartupProjectFile().empty()&&!m_MaterialLab&&!m_MaterialInstanceLab&&
        !m_TranslucencyTest&&m_InstanceGridSize==0&&m_FaceShadowDemoModelPath.empty()&&
        m_NormalMapPath.empty()&&m_DisplacementMapPath.empty())
        OpenProjectFile(m_Project->StartupProjectFile());
    return true;
}

bool EditorApp::CreateStartupScene(const StartupSceneDefinition& scene)
{
    if (scene.basicScene && !m_MaterialLab && !m_MaterialInstanceLab &&
        !m_TranslucencyTest && m_InstanceGridSize == 0 && m_FaceShadowDemoModelPath.empty() &&
        m_NormalMapPath.empty() && m_DisplacementMapPath.empty())
        return CreateBasicStartupScene(scene);
    const CubeGeometry cube = CreateCubeGeometry(scene.cubeEdgeLength);
    const auto& vertices = cube.vertices;
    m_Engine.Scene().objCenter = cy::Vec3f(0.0f);
    const float rawDiameter = scene.cubeEdgeLength * std::sqrt(3.0f);
    const float modelDiameter = rawDiameter;
    m_Engine.Scene().modelDiameter = modelDiameter;
    Material mainMaterial;
    mainMaterial.SetName("Default Material");
    if (!m_NormalMapPath.empty()) {
        mainMaterial.SetNormalMap(Texture2D::Load(m_Renderer->Device(),
            m_NormalMapPath, TextureColorSpace::Linear));
    }
    if (!m_DisplacementMapPath.empty()) {
        mainMaterial.SetDisplacementMap(Texture2D::Load(m_Renderer->Device(),
            m_DisplacementMapPath, TextureColorSpace::Linear));
    }
    PrimitiveBounds bounds;
    bounds.center = m_Engine.Scene().objCenter;
    bounds.radius = rawDiameter * 0.5f;
    const std::uint32_t effectiveGridSize = (m_MaterialLab || m_MaterialInstanceLab)
        ? 2
        : (m_InstanceGridSize == 0 ? 1 : m_InstanceGridSize);
    const float instanceSpacing = std::max(rawDiameter * 1.25f, 0.001f);
    const std::vector<cy::Vec3f> instanceOffsets = BuildInstanceGridOffsets(
        effectiveGridSize, instanceSpacing);
    m_Engine.Scene().sceneRadius = CalculateInstanceGridSceneRadius(
        effectiveGridSize, instanceSpacing, bounds.radius);
    const float gridHalfSpan =
        static_cast<float>(effectiveGridSize - 1) * instanceSpacing * 0.5f;
    m_Engine.Scene().defaultBoundsMin = cy::Vec3f(-scene.cubeEdgeLength * 0.5f) - m_Engine.Scene().objCenter - cy::Vec3f(gridHalfSpan, gridHalfSpan, 0.0f);
    m_Engine.Scene().defaultBoundsMax = cy::Vec3f(scene.cubeEdgeLength * 0.5f) - m_Engine.Scene().objCenter + cy::Vec3f(gridHalfSpan, gridHalfSpan, 0.0f);
    m_Engine.Scene().groundY = (cy::Vec3f(-scene.cubeEdgeLength * 0.5f).y - m_Engine.Scene().objCenter.y - gridHalfSpan -
        rawDiameter * 0.02f);
    const float cameraDistance = modelDiameter > 0.0f
        ? std::max(modelDiameter * 1.25f, m_Engine.Scene().sceneRadius * 2.5f)
        : 5.0f;
    m_Camera.SetDistance(cameraDistance);
    m_Camera.SetClipPlanes(
        0.1f,
        std::max(1000.0f, cameraDistance + m_Engine.Scene().sceneRadius * 1.5f));

    m_Engine.Scene().models.emplace_back();
    SceneModel& defaultModel = m_Engine.Scene().models.back();
    defaultModel.sourceCubeEdge = scene.cubeEdgeLength;
    defaultModel.sourceMeshNames = {"Cube"};
    defaultModel.transform = scene.cubeTransform;
    const cy::Matrix4f defaultRoot = defaultModel.transform.ToMatrix();
    m_Engine.Scene().postProcess.ssaoRadius = 0.5f;
    m_Engine.Scene().postProcess.ssaoBias = 0.025f;
    defaultModel.id = ModelId{m_Engine.Scene().nextModelId++};
    m_Engine.Scene().defaultModelId = defaultModel.id;
    defaultModel.name = m_MaterialInstanceLab ? "Material Instance Lab" :
        (m_MaterialLab ? "Material Lab" : scene.cubeName);

    const MeshHandle instanceMesh = m_Renderer->CreateMesh(vertices, cube.indices);
    if (!instanceMesh.IsValid())
    {
        std::cerr << "[Error] Failed to create the shared surface mesh."
                  << std::endl;
        return false;
    }
    defaultModel.meshes.push_back(instanceMesh);
    MaterialHandle instanceMaterial;

    if (m_MaterialInstanceLab)
    {
        // 三个分段共享 Mesh，使用一个基础材质和两个实例；资源全部由此模型跟踪清理。
        mainMaterial.SetName("Parent Surface");
        mainMaterial.SetAlbedoMap(nullptr);
        mainMaterial.SetSpecularMap(nullptr);
        mainMaterial.SetOcclusionRoughnessMetallicMap(nullptr);
        mainMaterial.GetProperties().baseColor = cy::Vec3f(0.7f, 0.7f, 0.7f);
        mainMaterial.GetProperties().roughness = 0.35f;
        const MaterialHandle parent = m_Renderer->CreateMaterial(std::move(mainMaterial));
        if (!parent.IsValid()) return false;
        defaultModel.materials.push_back(parent);
        for (int i = 0; i < 2; ++i)
        {
            const MaterialHandle child = m_Renderer->CreateMaterialInstance(parent,
                i == 0 ? "Instance A" : "Instance B");
            if (!child.IsValid()) return false;
            defaultModel.materials.push_back(child);
        }
        Renderer::MaterialSnapshot inherited;
        m_Renderer->GetMaterialSnapshot(parent, inherited);
        inherited.properties.baseColor = cy::Vec3f(0.85f, 0.12f, 0.08f);
        m_Renderer->UpdateMaterialOverride(defaultModel.materials[1],
            MaterialParameter::BaseColor, inherited.properties);
        for (std::size_t i = 0; i < 3; ++i)
        {
            const auto material = defaultModel.materials[i];
            const auto localToWorld = cy::Matrix4f::Translation(instanceOffsets[i]) *
                cy::Matrix4f::Translation(-m_Engine.Scene().objCenter);
            const auto primitive = m_Renderer->AddPrimitive(instanceMesh, material, defaultRoot * localToWorld, bounds);
            if (primitive == InvalidPrimitiveId) return false;
            defaultModel.sections.push_back({primitive, localToWorld, bounds, material,
                i == 0 ? "Parent" : i == 1 ? "Instance A" : "Instance B"});
        }
        std::cout << "[MaterialInstanceLab] mesh=1 parent=1 instances=2; A overrides Base Color"
                  << std::endl;
    }
    else if (m_MaterialLab)
    {
        struct LabMaterialDescription
        {
            const char* name;
            cy::Vec3f baseColor;
            float metallic;
            float roughness;
            float normalScale;
            bool useNormalMap;
            std::shared_ptr<Texture2D> ormMap;
        };

        const std::shared_ptr<Texture2D> copperOrm =
            CreateOrmCheckerTexture(m_Renderer->Device(), 42, 86, 255);
        const std::shared_ptr<Texture2D> roughMetalOrm =
            CreateOrmCheckerTexture(m_Renderer->Device(), 190, 235, 255);
        const std::array<LabMaterialDescription, 4> descriptions = {{
            {"Copper", {0.95f, 0.42f, 0.18f}, 1.0f, 1.0f, 1.0f,
                false, copperOrm},
            {"Plastic", {0.04f, 0.32f, 0.92f}, 0.0f, 0.28f, 1.0f,
                false, nullptr},
            {"Ceramic", {0.92f, 0.90f, 0.82f}, 0.0f, 0.18f, 0.55f,
                true, nullptr},
            {"RoughMetal", {0.38f, 0.42f, 0.46f}, 1.0f, 1.0f, 1.0f,
                false, roughMetalOrm}
        }};

        for (std::size_t index = 0; index < descriptions.size(); ++index)
        {
            const LabMaterialDescription& description = descriptions[index];
            Material material = mainMaterial;
            // 为测试材质提供可辨认名称，Inspector 与材质编辑器共享此名称。
            material.SetName(description.name);
            MaterialProperties& properties = material.GetProperties();
            properties.baseColor = description.baseColor;
            properties.specularColor = cy::Vec3f(1.0f, 1.0f, 1.0f);
            properties.metallic = description.metallic;
            properties.roughness = description.roughness;
            properties.ambientOcclusion = 1.0f;
            properties.normalScale = description.normalScale;
            properties.environmentReflectivity = 0.35f;

            material.SetAlbedoMap(nullptr);
            material.SetSpecularMap(nullptr);
            material.SetDisplacementMap(nullptr);
            if (!description.useNormalMap)
                material.SetNormalMap(nullptr);
            material.SetOcclusionRoughnessMetallicMap(description.ormMap);

            const MaterialHandle materialHandle =
                m_Renderer->CreateMaterial(std::move(material));
            if (!materialHandle.IsValid())
                continue;
            defaultModel.materials.push_back(materialHandle);

            const cy::Matrix4f localToWorld =
                cy::Matrix4f::Translation(instanceOffsets[index]) *
                cy::Matrix4f::Translation(-m_Engine.Scene().objCenter);
            const PrimitiveId primitiveId = m_Renderer->AddPrimitive(
                instanceMesh,
                materialHandle,
                defaultRoot * localToWorld,
                bounds);
            if (primitiveId != InvalidPrimitiveId)
            {
                defaultModel.sections.push_back({
                    primitiveId, localToWorld, bounds, materialHandle,
                    description.name });
            }
        }

        std::cout
            << "[MaterialLab] materials=4 meshResource=" << instanceMesh.id
            << " layout=2x2 orm=R:AO,G:Roughness,B:Metallic"
            << std::endl;
    }
    else
    {
        instanceMaterial = m_Renderer->CreateMaterial(std::move(mainMaterial));
        if (!instanceMaterial.IsValid())
        {
            std::cerr << "[Error] Failed to create the surface material."
                      << std::endl;
            return false;
        }
        defaultModel.materials.push_back(instanceMaterial);

        for (const cy::Vec3f& offset : instanceOffsets)
        {
            const cy::Matrix4f localToWorld =
                cy::Matrix4f::Translation(offset) *
                cy::Matrix4f::Translation(-m_Engine.Scene().objCenter);
            const PrimitiveId primitiveId = m_Renderer->AddPrimitive(
                instanceMesh,
                instanceMaterial,
                defaultRoot * localToWorld,
                bounds);
            if (primitiveId != InvalidPrimitiveId)
            {
                defaultModel.sections.push_back({
                    primitiveId, localToWorld, bounds, instanceMaterial,
                    "Surface" });
            }
        }
    }
    if (defaultModel.sections.empty())
    {
        std::cerr << "[Error] Failed to submit the opaque instance scene."
                  << std::endl;
        return false;
    }
    if (m_InstanceGridSize > 0)
    {
        std::cout
            << "[InstanceBenchmark] grid=" << effectiveGridSize << "x"
            << effectiveGridSize
            << " instances=" << instanceOffsets.size()
            << " meshResource=" << instanceMesh.id
            << " materialResource=" << instanceMaterial.id
            << " sceneRadius=" << m_Engine.Scene().sceneRadius
            << std::endl;
    }

    if (m_TranslucencyTest)
        CreateTranslucencyTestScene();

    // Initialization is committed before the event loop. Shutdown rolls back all
    // tracked resources on any failure, while the GL context is still alive.
    SceneLight light;
    light.name = scene.lightName;
    light.transform.rotationDegrees = scene.lightRotation;
    light.proxy.type = LightType::Directional;
    light.proxy.color = scene.lightColor;
    light.proxy.intensity = scene.lightIntensity;
    light.proxy.castsShadow = scene.castsShadow;
    light.proxy.id = m_Renderer->AddLight(light.proxy);
    if (!light.IsValid()) return false;
    m_Engine.Scene().mainLightId = light.proxy.id;
    m_Engine.Scene().lights.push_back(light);
    ApplySceneLightTransform(m_Engine.Scene().lights.back(), *m_Renderer);
    m_Engine.Scene().camera = scene.camera;
    m_Engine.Scene().camera.id = CameraId{1};
    m_Engine.Scene().reflectionGroundEnabled = m_MaterialLab || m_MaterialInstanceLab ||
        m_TranslucencyTest || m_InstanceGridSize > 0 || !m_FaceShadowDemoModelPath.empty();
    m_EditorSelection.SelectModel(m_Engine.Scene().defaultModelId);
    if (scene.selectedObject == "directionalLight") m_EditorSelection.SelectLight(m_Engine.Scene().mainLightId);
    if (scene.selectedObject == "camera") m_EditorSelection.SelectCamera(m_Engine.Scene().camera.id);
    m_Camera.ProcessMouseOrbit(78.5398f, 45.0f);
    m_PendingFocusSelection.SelectModel(m_Engine.Scene().defaultModelId);
    std::cout << "[StartupScene] Cube edge=" << scene.cubeEdgeLength
        << " m; Directional Light; Camera; 1 unit=1 m" << std::endl;

    if (!LoadStartupFaceShadowDemo())
        return false;

    return true;
}

bool EditorApp::CreateBasicStartupScene(const StartupSceneDefinition& definition)
{
    const BasicStartupSceneDefinition& basic = *definition.basicScene;
    SceneState& scene = m_Engine.Scene();
    for (const StartupModelDefinition* entry : {&basic.ground, &basic.monkey})
    {
        auto imported = AssetImport::FbxModelImporter::Import(entry->path);
        if (!imported)
        {
            std::cerr << "[StartupScene] Failed to import '" << entry->path.u8string()
                << "': " << imported.error << std::endl;
            return false;
        }
        imported.model.name = entry->name;
        for (auto& material : imported.model.materials)
        {
            material.baseColor = entry->baseColor;
            material.metallic = 0.0f;
            material.roughness = entry->roughness;
            material.opacity = 1.0f;
        }
        std::string error;
        ModelId id;
        if (!CommitImportedModel(imported.model, error, id))
        {
            std::cerr << "[StartupScene] Failed to submit '" << entry->name
                << "': " << error << std::endl;
            return false;
        }
        SceneModel* model = FindSceneModel(scene.models, id);
        model->transform = entry->transform;
        ApplySceneModelTransform(*model, *m_Renderer);
        for (const auto& warning : imported.model.warnings)
            std::cerr << "[StartupScene] " << entry->name << ": " << warning << std::endl;
        if (entry == &basic.monkey)
        {
            scene.defaultModelId = id;
            scene.defaultBoundsMin = imported.model.boundsMin;
            scene.defaultBoundsMax = imported.model.boundsMax;
        }
    }
    const SceneModel* monkey = FindSceneModel(scene.models, scene.defaultModelId);
    const PrimitiveBounds monkeyBounds = monkey->GetWorldBounds();
    scene.objCenter = monkeyBounds.center;
    scene.modelDiameter = monkeyBounds.radius * 2.0f;
    scene.sceneRadius = GetSceneWorldBounds(scene.models).radius;
    scene.groundY = basic.ground.transform.position.y;
    // The visible FBX floor is an ordinary PBR primitive, without the legacy
    // mirror ground used by material/instancing validation modes.
    scene.reflectionGroundEnabled = false;
    m_Engine.Scene().postProcess.ssaoRadius = 0.5f;
    m_Engine.Scene().postProcess.ssaoBias = 0.025f;

    SceneLight light;
    light.name = basic.pointLightName;
    light.transform.position = basic.pointLightPosition;
    light.proxy.type = LightType::Point;
    light.proxy.color = basic.pointLightColor;
    light.proxy.intensity = basic.pointLightIntensity;
    light.proxy.range = basic.pointLightRange;
    // Point shadows are not implemented by the current shadow pipeline.
    light.proxy.castsShadow = false;
    light.proxy.id = m_Renderer->AddLight(light.proxy);
    if (!light.IsValid()) return false;
    scene.mainLightId = light.proxy.id;
    scene.lights.push_back(std::move(light));
    ApplySceneLightTransform(scene.lights.back(), *m_Renderer);
    scene.camera = definition.camera;
    scene.camera.id = CameraId{1};

    // Frame the Monkey rather than fitting the 100-meter ground. Preserve an
    // orbit camera at the authored eye position, so navigation works on launch.
    const cy::Vec3f offset = scene.camera.transform.position - monkeyBounds.center;
    const float distance = offset.Length();
    if (distance <= 0.001f)
    {
        std::cerr << "[StartupScene] Camera coincides with the Monkey center." << std::endl;
        return false;
    }
    m_Camera.SetTarget(monkeyBounds.center);
    m_Camera.SetDistance(distance);
    m_Camera.SetClipPlanes(scene.camera.nearPlane, scene.camera.farPlane);
    m_Camera.ProcessMouseOrbit(std::atan2(offset.x, offset.z) / 0.01f,
        std::asin(std::clamp(offset.y / distance, -1.0f, 1.0f)) / 0.01f);
    m_EditorSelection.SelectModel(scene.defaultModelId);
    if (definition.selectedObject == "ground")
        m_EditorSelection.SelectModel(scene.models.front().id);
    if (definition.selectedObject == "pointLight") m_EditorSelection.SelectLight(scene.mainLightId);
    if (definition.selectedObject == "camera") m_EditorSelection.SelectCamera(scene.camera.id);
    std::cout << "[StartupScene] Ground + Monkey + Point Light + Camera; 1 unit=1 m"
        << std::endl;
    return true;
}

void EditorApp::CreateTranslucencyTestScene()
{
    if (m_Renderer == nullptr || m_Engine.Scene().modelDiameter <= 0.0f)
        return;

    const float halfWidth = m_Engine.Scene().modelDiameter * 0.24f;
    const float halfHeight = m_Engine.Scene().modelDiameter * 0.38f;
    const cy::Vec3f normal(0.0f, 0.0f, 1.0f);
    const cy::Vec4f tangent(1.0f, 0.0f, 0.0f, 1.0f);

    // 三张平面使用相同局部网格，并在世界空间中错开位置；重叠区域用于观察混合顺序。
    const std::vector<Vertex> planeVertices = {
        {{-halfWidth, -halfHeight, 0.0f}, normal, {0.0f, 0.0f}, tangent},
        {{ halfWidth, -halfHeight, 0.0f}, normal, {1.0f, 0.0f}, tangent},
        {{ halfWidth,  halfHeight, 0.0f}, normal, {1.0f, 1.0f}, tangent},
        {{-halfWidth, -halfHeight, 0.0f}, normal, {0.0f, 0.0f}, tangent},
        {{ halfWidth,  halfHeight, 0.0f}, normal, {1.0f, 1.0f}, tangent},
        {{-halfWidth,  halfHeight, 0.0f}, normal, {0.0f, 1.0f}, tangent}
    };

    struct TestLayer
    {
        cy::Vec3f color;
        float opacity;
        cy::Vec3f position;
    };

    const TestLayer layers[] = {
        {cy::Vec3f(0.08f, 0.85f, 0.95f), 0.28f,
            cy::Vec3f(-m_Engine.Scene().modelDiameter * 0.16f, 0.0f, -m_Engine.Scene().modelDiameter * 0.28f)},
        {cy::Vec3f(0.95f, 0.10f, 0.55f), 0.42f,
            cy::Vec3f(0.0f, 0.0f, 0.0f)},
        {cy::Vec3f(0.95f, 0.78f, 0.08f), 0.58f,
            cy::Vec3f(m_Engine.Scene().modelDiameter * 0.16f, 0.0f, m_Engine.Scene().modelDiameter * 0.28f)}
    };

    PrimitiveBounds bounds;
    bounds.radius = std::sqrt(
        halfWidth * halfWidth + halfHeight * halfHeight);
    const MeshHandle planeMesh = m_Renderer->CreateMesh(planeVertices);
    if (!planeMesh.IsValid())
        return;

    const std::vector<unsigned char> alphaChecker = {
        255, 255, 255, 255,  255, 255, 255, 64,
        255, 255, 255, 64,   255, 255, 255, 255
    };
    const std::shared_ptr<Texture2D> alphaTexture = Texture2D::CreateRGBA8(m_Renderer->Device(),
        2, 2, alphaChecker, TextureColorSpace::SRGB);

    for (const TestLayer& layer : layers)
    {
        Material material;
        MaterialProperties& properties = material.GetProperties();
        properties.baseColor = layer.color;
        properties.roughness = 0.65f;
        properties.environmentReflectivity = 0.05f;
        properties.opacity = layer.opacity;
        material.SetBlendMode(BlendMode::AlphaBlend);
        material.SetAlbedoMap(alphaTexture);
        const MaterialHandle materialHandle =
            m_Renderer->CreateMaterial(std::move(material));
        if (!materialHandle.IsValid())
            continue;

        // 三个 Primitive 共享同一份 VAO/VBO，只保留各自的材质与 Transform。
        // 测试平面不投射阴影，避免把透明混合验收与 Alpha 阴影需求混在一起。
        m_Renderer->AddPrimitive(
            planeMesh,
            materialHandle,
            cy::Matrix4f::Translation(layer.position),
            bounds,
            false);
    }
}

/// @brief 更新应用程序状态
bool EditorApp::CommitImportedModel(
    AssetImport::ImportedModelData& model,
    std::string& error,
    ModelId& importedId)
{
    error.clear();
    importedId = {};
    if (m_Renderer == nullptr)
    {
        error = "Renderer is unavailable.";
        return false;
    }
    if (model.meshes.empty() || model.materials.empty())
    {
        error = "The imported model has no renderable sections or materials.";
        return false;
    }

    SceneModel pendingModel;
    pendingModel.name = model.name;
    pendingModel.importedSizeMeters = model.boundsMax - model.boundsMin;
    pendingModel.sourceUnitMeters = model.sourceUnitMeters;
    pendingModel.sourcePath = std::filesystem::absolute(model.sourcePath);
    for (const auto& mesh : model.meshes) pendingModel.sourceMeshNames.push_back(mesh.name);
    pendingModel.meshes.reserve(model.meshes.size());
    pendingModel.materials.reserve(model.materials.size());
    pendingModel.sections.reserve(model.meshes.size());

    const auto fail = [this, &pendingModel, &error](std::string message)
    {
        error = std::move(message);
        DestroyModelResources(pendingModel);
        return false;
    };

    try
    {
        // Decoded CPU images can serve color and data roles. Only share uploads
        // within this import when both source index and sampling space match.
        std::map<std::pair<std::uint32_t, TextureColorSpace>,
            std::shared_ptr<Texture2D>> textures;
        const auto uploadTexture = [this, &model, &textures, &error](
            std::uint32_t index, MaterialTextureSlot slot) -> std::shared_ptr<Texture2D>
        {
            if (index >= model.textures.size())
            {
                error = "Imported material references an invalid texture index.";
                return {};
            }
            const auto colorSpace = GetRequiredMaterialTextureColorSpace(slot);
            const auto key = std::make_pair(index, colorSpace);
            const auto cached = textures.find(key);
            if (cached != textures.end())
                return cached->second;
            const AssetImport::ImportedTextureData& importedTexture =
                model.textures[index];
            if (!importedTexture.image.IsValid())
            {
                error = "Imported texture data is invalid: " + importedTexture.name;
                return {};
            }

            auto texture = Texture2D::CreateRGBA8(m_Renderer->Device(),
                importedTexture.image.width,
                importedTexture.image.height,
                importedTexture.image.pixels,
                colorSpace);
            if (!texture)
            {
                error = "GPU texture creation failed: " + importedTexture.name;
                return {};
            }
            if (importedTexture.reconstruction)
                texture->SetReconstructionSource(*importedTexture.reconstruction);
            else if (importedTexture.embedded && importedTexture.sourceId && !model.sourcePath.empty())
                texture->SetSource(std::filesystem::absolute(model.sourcePath), importedTexture.sourceId);
            else if (!importedTexture.embedded && !importedTexture.sourcePath.empty())
                texture->SetSource(std::filesystem::absolute(importedTexture.sourcePath));
            textures.emplace(key, texture);
            return texture;
        };

        for (const AssetImport::ImportedMaterialData& importedMaterial :
            model.materials)
        {
            Material material;
            material.SetName(importedMaterial.name);
            MaterialProperties& properties = material.GetProperties();
            properties.baseColor = importedMaterial.baseColor;
            properties.metallic = importedMaterial.metallic;
            properties.roughness = importedMaterial.roughness;
            properties.ambientOcclusion = importedMaterial.ambientOcclusion;
            properties.opacity = importedMaterial.opacity;
            if (importedMaterial.opacity < 0.999f)
                material.SetBlendMode(BlendMode::AlphaBlend);

            const std::array<std::pair<MaterialTextureSlot, std::uint32_t>, 3> bindings = {{
                {MaterialTextureSlot::BaseColor, importedMaterial.baseColorTexture},
                {MaterialTextureSlot::Normal, importedMaterial.normalTexture},
                {MaterialTextureSlot::OcclusionRoughnessMetallic, importedMaterial.ormTexture}}};
            for (const auto& binding : bindings)
            {
                if (binding.second == AssetImport::InvalidImportedTextureIndex)
                    continue;
                auto texture = uploadTexture(binding.second, binding.first);
                if (!texture)
                    return fail(error + " Material: " + importedMaterial.name);
                const AssetImport::ImportedTextureData& importedTexture =
                    model.textures[binding.second];
                std::string sourceLabel;
                if (importedTexture.reconstruction)
                {
                    const auto& source = *importedTexture.reconstruction;
                    sourceLabel = source.kind == AssetImport::TextureSourceKind::Orm
                        ? "Generated ORM"
                        : source.image.embeddedId
                            ? "Embedded: " + importedTexture.name
                            : source.image.path.u8string();
                }
                else
                    sourceLabel = importedTexture.embedded ? "Embedded: " + importedTexture.name
                        : importedTexture.sourcePath.u8string();
                if (!material.SetTexture(
                        binding.first,
                        std::move(texture),
                        sourceLabel))
                {
                    return fail(
                        "Unable to bind an imported texture for material: " +
                        importedMaterial.name);
                }
            }

            const MaterialHandle materialHandle =
                m_Renderer->CreateMaterial(std::move(material));
            if (!materialHandle.IsValid())
            {
                return fail(
                    "Renderer material creation failed: " +
                    importedMaterial.name);
            }
            pendingModel.materials.push_back(materialHandle);
        }

        for (const AssetImport::ImportedMeshData& importedMesh : model.meshes)
        {
            if (importedMesh.materialIndex >=
                pendingModel.materials.size())
            {
                return fail(
                    "Imported mesh references an invalid material: " +
                    importedMesh.name);
            }

            const MeshHandle meshHandle = m_Renderer->CreateMesh(
                importedMesh.vertices,
                importedMesh.indices);
            if (!meshHandle.IsValid())
            {
                return fail(
                    "Renderer mesh creation failed: " + importedMesh.name);
            }
            pendingModel.meshes.push_back(meshHandle);

            PrimitiveBounds bounds;
            bounds.center = importedMesh.boundsCenter;
            bounds.radius = importedMesh.boundsRadius;
            // 名称分配可能抛异常；在提交 Primitive 前完成，确保回滚能追踪每个代理。
            SceneModelSection section{
                InvalidPrimitiveId, importedMesh.localToWorld, bounds,
                pendingModel.materials[importedMesh.materialIndex], importedMesh.name };
            const PrimitiveId primitiveId = m_Renderer->AddPrimitive(
                meshHandle,
                pendingModel.materials[importedMesh.materialIndex],
                importedMesh.localToWorld,
                bounds);
            if (primitiveId == InvalidPrimitiveId)
            {
                return fail(
                    "Renderer primitive submission failed: " +
                    importedMesh.name);
            }
            section.primitiveId = primitiveId;
            pendingModel.sections.push_back(std::move(section));
        }
    }
    catch (const std::exception& exception)
    {
        return fail(
            std::string("Imported resource creation failed: ") +
            exception.what());
    }

    if (m_Engine.Scene().nextModelId == 0)
        return fail("Scene model ID space exhausted.");
    pendingModel.id = ModelId{m_Engine.Scene().nextModelId};
    try
    {
        pendingModel.name = MakeUniqueModelName(m_Engine.Scene().models, model.name);
        if (pendingModel.name.empty())
            return fail("Scene model name space exhausted.");
        m_Engine.Scene().models.push_back(std::move(pendingModel));
    }
    catch (const std::exception& exception)
    {
        return fail(std::string("Unable to add the model to the scene: ") +
            exception.what());
    }
    importedId = ModelId{m_Engine.Scene().nextModelId++};

    std::cout
        << "[AssetImport] model='" << model.name
        << "' sections=" << model.meshes.size()
        << " materials=" << model.materials.size()
        << " textures=" << model.textures.size()
        << " sourceUnitMeters=" << (model.sourceUnitMeters ? std::to_string(*model.sourceUnitMeters) : "unknown")
        << " sizeMeters=(" << (model.boundsMax - model.boundsMin).x
        << "," << (model.boundsMax - model.boundsMin).y
        << "," << (model.boundsMax - model.boundsMin).z << ")"
        << std::endl;
    return true;
}

bool EditorApp::LoadStartupFaceShadowDemo()
{
    if (m_FaceShadowDemoModelPath.empty())
        return true;

    const std::filesystem::path modelPath(m_FaceShadowDemoModelPath);
    const std::filesystem::path faceShadowPath(m_FaceShadowDemoTexturePath);

    AssetImport::ImageData faceShadowImage;
    std::string imageError;
    if (!AssetImport::ImageLoader::LoadFromFile(
            faceShadowPath, faceShadowImage, imageError))
    {
        std::cerr
            << "[FaceShadowDemo] Unable to load the Face Shadow texture: "
            << imageError << std::endl;
        return false;
    }

    AssetImport::ModelImportResult importResult =
        AssetImport::FbxModelImporter::Import(modelPath);
    if (!importResult)
    {
        std::cerr
            << "[FaceShadowDemo] Unable to import the startup FBX: "
            << importResult.error << std::endl;
        return false;
    }

    const auto materialIterator = std::find_if(
        importResult.model.materials.begin(),
        importResult.model.materials.end(),
        [this](const AssetImport::ImportedMaterialData& material)
        {
            return material.name == m_FaceShadowDemoMaterialName;
        });
    if (materialIterator == importResult.model.materials.end())
    {
        std::cerr
            << "[FaceShadowDemo] Material '"
            << m_FaceShadowDemoMaterialName
            << "' was not found in the startup FBX." << std::endl;
        return false;
    }
    const std::size_t materialIndex = static_cast<std::size_t>(
        std::distance(importResult.model.materials.begin(), materialIterator));

    std::string commitError;
    ModelId importedId;
    if (!CommitImportedModel(importResult.model, commitError, importedId))
    {
        std::cerr
            << "[FaceShadowDemo] Unable to upload the startup model: "
            << commitError << std::endl;
        return false;
    }
    // Startup demo keeps its historical single-character scene.
    for (std::size_t index = 0; index < m_Engine.Scene().models.size();)
    {
        if (m_Engine.Scene().models[index].id == importedId)
            ++index;
        else
            RemoveModel(m_Engine.Scene().models[index].id);
    }
    SceneModel* demoModel = FindSceneModel(m_Engine.Scene().models, importedId);
    if (demoModel == nullptr || materialIndex >= demoModel->materials.size())
    {
        std::cerr
            << "[FaceShadowDemo] Imported material mapping is invalid."
            << std::endl;
        return false;
    }

    // This asset contains four weapon helper sections far from the character.
    // Keep their primitives renderable, but prevent them from moving the
    // lighting/framing target away from the face-shadow subject.
    if (importResult.model.meshes.size() == demoModel->sections.size() &&
        importResult.model.meshes.size() >= 4)
    {
        auto median = [](std::vector<float> values)
        {
            const std::size_t middle = values.size() / 2;
            std::nth_element(
                values.begin(), values.begin() + middle, values.end());
            return values[middle];
        };
        std::vector<float> centerX;
        std::vector<float> centerY;
        std::vector<float> centerZ;
        centerX.reserve(importResult.model.meshes.size());
        centerY.reserve(importResult.model.meshes.size());
        centerZ.reserve(importResult.model.meshes.size());
        for (const AssetImport::ImportedMeshData& mesh :
             importResult.model.meshes)
        {
            centerX.push_back(mesh.boundsCenter.x);
            centerY.push_back(mesh.boundsCenter.y);
            centerZ.push_back(mesh.boundsCenter.z);
        }
        const cy::Vec3f medianCenter(
            median(std::move(centerX)),
            median(std::move(centerY)),
            median(std::move(centerZ)));
        std::vector<float> distances;
        distances.reserve(importResult.model.meshes.size());
        for (const AssetImport::ImportedMeshData& mesh :
             importResult.model.meshes)
        {
            distances.push_back((mesh.boundsCenter - medianCenter).Length());
        }
        const float warningDistance = std::max(
            10.0f, median(distances) * 10.0f);
        std::size_t ignoredDistantSections = 0;
        for (std::size_t index = 0; index < distances.size(); ++index)
        {
            if (distances[index] <= warningDistance)
                continue;
            demoModel->sections[index].localBounds.radius = 0.0f;
            ++ignoredDistantSections;
        }
        if (ignoredDistantSections > 0)
        {
            std::cout
                << "[FaceShadowDemo] ignored " << ignoredDistantSections
                << " distant sections for lighting/framing bounds"
                << std::endl;
        }
    }

    const PrimitiveBounds demoBounds = demoModel->GetWorldBounds();
    if (demoBounds.radius > 0.0f)
    {
        m_Engine.Scene().objCenter = demoBounds.center;
        m_Engine.Scene().modelDiameter = demoBounds.radius * 2.0f;
        m_Engine.Scene().sceneRadius = demoBounds.radius;
        m_Engine.Scene().groundY = demoBounds.center.y - demoBounds.radius;
        m_Camera.FocusBounds(demoBounds.center, demoBounds.radius);
        m_PendingFocusSelection.SelectModel(importedId);

        SceneLight* mainLight = FindSceneLight(m_Engine.Scene().mainLightId);
        if (mainLight != nullptr)
        {
            cy::Vec3f lightOffset = mainLight->transform.position -
                demoBounds.center;
            if (lightOffset.Length() <= 1.0e-4f)
                lightOffset = cy::Vec3f(0.0f, 1.0f, 2.0f);
            lightOffset.Normalize();
            mainLight->transform.position = demoBounds.center +
                lightOffset * std::max(demoBounds.radius * 2.5f, 1.0f);
            mainLight->proxy.range = std::max(
                mainLight->proxy.range, demoBounds.radius * 6.0f);
            ApplySceneLightTransform(*mainLight, *m_Renderer);
        }
    }

    const std::shared_ptr<Texture2D> faceShadowTexture =
        Texture2D::CreateRGBA8(m_Renderer->Device(),
            faceShadowImage.width,
            faceShadowImage.height,
            faceShadowImage.pixels,
            TextureColorSpace::Linear);
    if (!faceShadowTexture)
    {
        std::cerr
            << "[FaceShadowDemo] GPU texture creation failed."
            << std::endl;
        return false;
    }
    faceShadowTexture->SetSource(std::filesystem::absolute(faceShadowPath));

    const MaterialHandle materialHandle =
        demoModel->materials[materialIndex];
    Renderer::MaterialSnapshot snapshot;
    if (!m_Renderer->GetMaterialSnapshot(materialHandle, snapshot))
    {
        std::cerr
            << "[FaceShadowDemo] Unable to read the imported face material."
            << std::endl;
        return false;
    }

    MaterialProperties properties = snapshot.properties;
    properties.shadingModel = ShadingModel::Toon;
    properties.faceShadowEnabled = true;
    if (!m_Renderer->UpdateMaterial(
            materialHandle, properties, snapshot.blendMode) ||
        !m_Renderer->UpdateMaterialTexture(
            materialHandle,
            MaterialTextureSlot::FaceShadow,
            faceShadowTexture,
            faceShadowPath.u8string()))
    {
        std::cerr
            << "[FaceShadowDemo] Unable to configure the face material."
            << std::endl;
        return false;
    }

    for (const std::string& warning : importResult.model.warnings)
        std::cout << "[FaceShadowDemo] warning: " << warning << std::endl;
    std::cout
        << "[FaceShadowDemo] model='" << modelPath.u8string()
        << "' material='" << m_FaceShadowDemoMaterialName
        << "' texture='" << faceShadowPath.u8string()
        << "' colorSpace=Linear faceForward=(0,0,1) faceRight=(1,0,0)"
        << std::endl;
    return true;
}

void EditorApp::DestroyModelResources(SceneModel& model)
{
    if (m_Renderer) DestroySceneModelResources(model, *m_Renderer);
}

void EditorApp::ApplyMaterialEditRequest(const MaterialEditRequest& request)
{
    if (request.action == MaterialEditAction::None) return;
    SceneModel* model = FindSceneModel(m_Engine.Scene().models, request.model);
    Renderer::MaterialSnapshot snapshot;
    if (!model || !m_Renderer->GetMaterialSnapshot(request.material, snapshot)) return;
    std::vector<PrimitiveId> ids;
    for (const auto& section : model->sections)
        if (section.material.id == request.material.id) ids.push_back(section.primitiveId);
    if (ids.empty()) return;
    MaterialHandle created;
    try
    {
        MaterialHandle replacement = snapshot.parent;
        if (request.action == MaterialEditAction::CreateInstance)
        {
            if (snapshot.isInstance) return;
            // 所有可能分配的跟踪容器先准备，避免成功替换后无法记录新资源。
            model->materials.reserve(model->materials.size() + 1);
            created = m_Renderer->CreateMaterialInstance(request.material);
            replacement = created;
        }
        else if (!snapshot.isInstance) return;
        if (!replacement.IsValid() ||
            !m_Renderer->ReplacePrimitiveMaterials(ids, request.material, replacement))
        {
            if (created.IsValid()) m_Renderer->DestroyMaterial(created);
            std::cerr << "[MaterialInstance] Model binding rejected." << std::endl;
            return;
        }
        for (auto& section : model->sections)
            if (section.material.id == request.material.id) section.material = replacement;
        if (created.IsValid()) model->materials.push_back(created);
        m_MaterialSelection.Select(model->id, replacement);
        if (snapshot.isInstance && m_Renderer->DestroyMaterial(request.material))
            model->materials.erase(std::remove_if(model->materials.begin(), model->materials.end(),
                [&](MaterialHandle h) { return h.id == request.material.id; }), model->materials.end());
    }
    catch (const std::exception& error)
    {
        if (created.IsValid()) m_Renderer->DestroyMaterial(created);
        std::cerr << "[MaterialInstance] " << error.what() << std::endl;
    }
}

bool EditorApp::RemoveModel(ModelId id)
{
    const auto found = std::find_if(m_Engine.Scene().models.begin(), m_Engine.Scene().models.end(),
        [id](const SceneModel& model) { return model.id == id; });
    if (found == m_Engine.Scene().models.end())
        return false;
    if (found->actor.IsValid()) {
        const bool removed=DestroySceneActor(m_Engine.Scene(),*m_Renderer,found->actor);
        if (!m_EditorSelection.ResolveActor(m_Engine.Scene()).IsValid()) m_EditorSelection.Clear();
        return removed;
    }
    if (m_EditorSelection.IsModelSelected(id))
        m_EditorSelection.Clear();
    if (m_Engine.Scene().defaultModelId == id)
        m_Engine.Scene().defaultModelId = {};
    DestroyModelResources(*found);
    m_Engine.Scene().models.erase(found);
    return true;
}

SceneLight* EditorApp::FindSceneLight(std::uint32_t id)
{
    const auto iterator = std::find_if(
        m_Engine.Scene().lights.begin(), m_Engine.Scene().lights.end(),
        [id](const SceneLight& light) { return light.proxy.id == id; });
    return iterator == m_Engine.Scene().lights.end() ? nullptr : &*iterator;
}

const SceneLight* EditorApp::FindSceneLight(std::uint32_t id) const
{
    const auto iterator = std::find_if(
        m_Engine.Scene().lights.begin(), m_Engine.Scene().lights.end(),
        [id](const SceneLight& light) { return light.proxy.id == id; });
    return iterator == m_Engine.Scene().lights.end() ? nullptr : &*iterator;
}

void EditorApp::Update() {
    if (!m_AssetImportPanel)
        return;

    m_AssetImportPanel->Update();
    std::optional<AssetImport::ModelImportResult> completedImport =
        m_AssetImportPanel->TakeCompletedImport();
    if (!completedImport)
        return;

    std::string error;
    ModelId importedId;
    if (CommitImportedModel(completedImport->model, error, importedId))
    {
        m_EditorSelection.SelectModel(importedId);
        m_SceneCameraView = false;
        m_PendingFocusSelection = m_EditorSelection;
        m_AssetImportPanel->ReportCommitSuccess(
            completedImport->model,
            FindSceneModel(m_Engine.Scene().models, importedId)->sections.size());
        if (m_Workspace)
            m_Workspace->AddLog("FBX imported: " +
                completedImport->model.sourcePath.filename().string());
    }
    else
    {
        if (m_Workspace)
            m_Workspace->AddLog("FBX import failed: " + error);
        m_AssetImportPanel->ReportCommitFailure(std::move(error));
    }
}

/// @brief 渲染应用程序
void EditorApp::Render() {
    // 先建立 ImGui 布局，取得中央视口的实际像素尺寸，再提交渲染帧。
    if (!m_Engine.Scene().camera.id.IsValid()) m_SceneCameraView=false;
    UpdateAppearance();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    void* nativeWindowHandle = GetNativeWindowHandle(m_Window);
    m_Workspace->BeginFrame(*m_AssetImportPanel, nativeWindowHandle, m_Appearance);
    HandleSceneFileActions(nativeWindowHandle);
    const EditorViewportRegion viewport = m_Workspace->BeginViewport([&] {
        m_ViewportController->DrawToolbar(m_EditorSelection, m_Engine.Scene().lights, m_SceneCameraView);
    });
    if (viewport.visible && viewport.pixelWidth > 0 && viewport.pixelHeight > 0)
    {
        m_Camera.SetAspectRatio(static_cast<float>(viewport.pixelWidth) /
            static_cast<float>(viewport.pixelHeight));
        if (m_PendingFocusSelection.type != EditorSelectionType::None)
        {
            m_ViewportController->FocusSelection(m_Camera, m_PendingFocusSelection,
                m_Engine.Scene().models, m_Engine.Scene().lights, &m_Engine.Scene().camera, &m_Engine.Scene().world);
            m_PendingFocusSelection.Clear();
        }
    }

    if (m_Workspace->ShowRenderSettings())
        DrawRenderSettingsPanel(m_Engine.Scene().postProcess, [&] {
            DrawShadowSettingsControls(m_Engine.Scene(),*m_Renderer);
            DrawEnvironmentControls(m_Engine.Scene(),*m_Renderer,nativeWindowHandle);
        });

    const SceneFrameSettings frameSettings{m_Renderer->IsTessellationEnabled(),
        m_Renderer->GetDisplacementScale(), m_Renderer->AreEditorPrimitivesEnabled()};
    RenderFrameData frame = m_Engine.PrepareFrame(m_Camera,
        viewport.pixelWidth, viewport.pixelHeight, m_SceneCameraView, frameSettings);
    if (viewport.visible && m_Width > 0 && m_Height > 0)
        m_Engine.Render(frame);

    // GPU 最终纹理由 Renderer 持有；编辑器在同一帧将其显示到中央面板。
    m_Workspace->DrawViewportImage(GetOpenGLTexture(m_Renderer->Device(),m_Renderer->GetFinalColorTexture()));
    if (viewport.visible)
        m_ViewportController->Draw(
            m_Camera, m_EditorSelection, m_Engine.Scene(), *m_Renderer, viewport, &m_Engine.Scene().camera, m_SceneCameraView);
    m_Workspace->EndViewport();

    m_AssetImportPanel->Draw(nativeWindowHandle);
    if (m_Workspace->ShowStatistics())
        m_StatisticsPanel->Draw(*m_Renderer);
    const auto removedActor=m_SceneHierarchyPanel->Draw(m_EditorSelection,m_Engine.Scene(),*m_Renderer);
    if (removedActor.IsValid()) {
        DestroySceneActor(m_Engine.Scene(),*m_Renderer,removedActor);
        if (!m_EditorSelection.ResolveActor(m_Engine.Scene()).IsValid()) m_EditorSelection.Clear();
        if (!m_PendingFocusSelection.ResolveActor(m_Engine.Scene()).IsValid()) m_PendingFocusSelection.Clear();
        if (!m_Engine.Scene().camera.id.IsValid()) m_SceneCameraView=false;
    }
    // 先应用场景选择/删除，再同步材质身份；即使材质面板关闭也清理失效选择。
    m_MaterialSelection.Synchronize(
        m_EditorSelection, m_Engine.Scene().models, m_Renderer->GetMaterialHandles());
    if (m_InspectorPanel->Draw(
            m_EditorSelection, m_Engine.Scene(), m_SceneCameraView, *m_Renderer,
            m_MaterialSelection))
        m_Workspace->OpenMaterialEditor();
    if (m_Workspace->ShowMaterialEditor())
    {
        // NextWindow 设置必须紧邻材质面板 Begin，避免被其它窗口消费。
        m_Workspace->PrepareMaterialEditor();
        MaterialEditRequest request;
        m_MaterialEditorPanel->Draw(
            *m_Renderer, m_EditorSelection, m_Engine.Scene().models, m_MaterialSelection, request,
            nativeWindowHandle);
        ApplyMaterialEditRequest(request);
    }
    m_Workspace->DrawBottomPanels(*m_AssetImportPanel);
    const auto* selectedActor = m_Engine.Scene().world.Find(m_EditorSelection.ResolveActor(m_Engine.Scene()));
    const std::string selectionLabel = selectedActor ? selectedActor->name + " selected" : "No selection";
    m_Workspace->DrawStatusBar(selectionLabel.c_str());
    EditorWidgets::DrawDockTabAccents();
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
    const auto background = EditorTheme::GetPalette().background;
    glClearColor(background.x, background.y, background.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}
void EditorApp::UpdateAppearance()
{
    float contentScaleX = 1.0f, contentScaleY = 1.0f;
    glfwGetWindowContentScale(m_Window, &contentScaleX, &contentScaleY);
    const float scale = std::clamp(contentScaleX * m_Appearance.uiScale, 0.75f, 4.0f);
    const bool resized = std::abs(scale - m_AppliedUiScale) > 0.01f;
    if (resized)
    {
        // Previous frame has already been submitted. Never rebuild a locked atlas.
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        std::string diagnostic;
        EditorFonts::Initialize(*ImGui::GetIO().Fonts,"assets/fonts/NotoSansSC.ttf",diagnostic,15.0f*scale);
        if (!diagnostic.empty()) m_Workspace->AddLog("[Warning] " + diagnostic);
    }
    if (resized || m_AppliedTheme != m_Appearance.theme)
    {
        EditorTheme::Apply(m_Appearance.theme,scale);
        m_AppliedUiScale = scale;
        m_AppliedTheme = m_Appearance.theme;
    }
}
/// @brief 关闭应用程序
void EditorApp::Shutdown() {

    if (!m_Initialized) return;

    m_StatisticsPanel.reset();
    m_Workspace.reset();
    m_MaterialEditorPanel.reset();
    m_AssetImportPanel.reset();
    m_ViewportController.reset();
    m_SceneHierarchyPanel.reset();
    m_InspectorPanel.reset();
    if (m_ImGuiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ImGuiInitialized = false;
    }
    m_Project.reset();

    m_Engine.Shutdown();
    m_Renderer = nullptr;

    m_Context.Shutdown();
    m_Window = nullptr;

    m_Initialized = false;

	std::cout << "[System] Engine Shutdown Successfully!" << std::endl;
}

// ==========================================
// 静态回调函数实现（使用 UserPointer 访问实例）
// ==========================================
void EditorApp::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    EditorApp* app = static_cast<EditorApp*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->m_Width = width;
        app->m_Height = height;

        // 相机投影宽高比由中央 Viewport 决定，而不是整个 GLFW 窗口。
        glViewport(0, 0, width, height);
    }
}

void EditorApp::MouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    EditorApp* app = static_cast<EditorApp*>(glfwGetWindowUserPointer(window));
    if (!app || !app->m_ViewportController || !app->m_Workspace)
        return;
    EditorPointerButton pointerButton;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        pointerButton = EditorPointerButton::Left;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        pointerButton = EditorPointerButton::Middle;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        pointerButton = EditorPointerButton::Right;
    else
        return;

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    const EditorViewportRegion& viewport = app->m_Workspace->GetViewportRegion();
    // ImGui 会捕获 Viewport 内的鼠标；按图像矩形和窗口悬停状态区分场景输入。
    if (action == GLFW_PRESS &&
        (!viewport.hovered || !viewport.Contains(x, y) || ImGuizmo::IsUsing()))
        return;
    const bool altDown =
        (mods & GLFW_MOD_ALT) != 0 ||
        glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    app->m_ViewportController->SetButtonState(
        pointerButton, action != GLFW_RELEASE, altDown, x, y);
}

void EditorApp::CursorPositionCallback(
    GLFWwindow* window,
    double xpos,
    double ypos)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    EditorApp* app = static_cast<EditorApp*>(glfwGetWindowUserPointer(window));
    if (!app || !app->m_ViewportController || !app->m_Workspace)
        return;

    const bool altDown =
        glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    const bool ctrlDown =
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const EditorViewportRegion& viewport = app->m_Workspace->GetViewportRegion();
    const bool scenePointerActive = app->m_ViewportController->HasPointerButtonDown();
    const bool inViewport = viewport.hovered && viewport.Contains(xpos, ypos);
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    app->m_ViewportController->ProcessPointerMove(
        xpos,
        ypos,
        altDown && !app->m_SceneCameraView && (inViewport || scenePointerActive),
        viewport.size.y,
        app->m_Camera,
        &deltaX,
        &deltaY);

    if (!inViewport && !scenePointerActive)
        return;
    if (!altDown && ctrlDown && app->m_ViewportController->IsLeftDown())
    {
        SceneLight* mainLight = app->FindSceneLight(app->m_Engine.Scene().mainLightId);
        if (mainLight)
        {
            auto transform=mainLight->transform;
            transform.rotationDegrees.x+=deltaY*0.5f;
            transform.rotationDegrees.y+=deltaX*0.5f;
            std::string error;
            if (!app->m_Engine.Scene().world.SetWorld(mainLight->actor,transform.ToMatrix(),error))
                app->m_Workspace->AddLog(error);
            SynchronizeSceneActors(app->m_Engine.Scene(),*app->m_Renderer);
        }
    }
}

void EditorApp::ScrollCallback(
    GLFWwindow* window,
    double xoffset,
    double yoffset)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    EditorApp* app = static_cast<EditorApp*>(glfwGetWindowUserPointer(window));
    if (!app || !app->m_ViewportController || !app->m_Workspace)
        return;
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    const EditorViewportRegion& viewport = app->m_Workspace->GetViewportRegion();
    if (!viewport.hovered || !viewport.Contains(x, y))
        return;
    if (app->m_SceneCameraView) return;
    app->m_ViewportController->ProcessScroll(
        static_cast<float>(yoffset), app->m_Camera);
}

void EditorApp::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_CharCallback(window, codepoint);
}

void EditorApp::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    EditorApp* app = static_cast<EditorApp*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
        return;
    }
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput)
        return;
    const bool viewportFocused = app->m_Workspace &&
        app->m_Workspace->GetViewportRegion().focused;
    if (!viewportFocused && key != GLFW_KEY_F6)
        return;
    if (action == GLFW_PRESS && app->m_ViewportController)
    {
        if (key == GLFW_KEY_W)
            app->m_ViewportController->SetOperation(ImGuizmo::TRANSLATE);
        else if (key == GLFW_KEY_E)
            app->m_ViewportController->SetOperation(ImGuizmo::ROTATE);
        else if (key == GLFW_KEY_R)
            app->m_ViewportController->SetOperation(ImGuizmo::SCALE);
        else if (key == GLFW_KEY_Q)
            app->m_ViewportController->ToggleSpace();
        else if (key == GLFW_KEY_F)
        {
            app->m_SceneCameraView = false;
            app->m_PendingFocusSelection = app->m_EditorSelection;
        }
    }
    // 着色器重载（F6）
    if (key == GLFW_KEY_F6 && action == GLFW_PRESS) {
        const bool loaded = app->m_Renderer->ReloadShaders();
        if (app->m_Workspace)
            app->m_Workspace->AddLog(loaded
                ? "Shaders reloaded successfully."
                : "Shader reload failed. See terminal for compiler details.");
    }

    // 投影模式切换（P）
    if (key == GLFW_KEY_KP_0 && action == GLFW_PRESS) {
        app->m_SceneCameraView = !app->m_SceneCameraView;
        app->m_ViewportController->CancelPointerInput();
    }
    if (key == GLFW_KEY_P && action == GLFW_PRESS && !app->m_SceneCameraView) {
        app->m_Camera.ToggleProjectionMode();
    }

    // L 键切换光源编辑器图元；只控制显示，不影响实际光照计算。
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        const bool enabled = !app->m_Renderer->AreEditorPrimitivesEnabled();
        app->m_Renderer->SetEditorPrimitivesEnabled(enabled);
        std::cout << "[Editor] Light Primitives: "
            << (enabled ? "ON" : "OFF") << std::endl;
    }

    // S 键切换阴影显示；Renderer 内部默认值为开启。
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        const bool enabled = !app->m_Renderer->IsShadowsEnabled();
        app->m_Renderer->SetShadowsEnabled(enabled);
        std::cout << "[Renderer] Shadows: "
            << (enabled ? "ON" : "OFF") << std::endl;
    }

    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        const bool enabled = !app->m_Renderer->IsTessellationEnabled();
        app->m_Renderer->SetTessellationEnabled(enabled);
        std::cout << "[Renderer] Mesh Tessellation: "
            << (enabled ? "ON" : "OFF")
            << std::endl;
    }

    // 退出（ESC）
    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
        const bool visible = !app->m_Renderer->IsTessellationWireframe();
        app->m_Renderer->SetTessellationWireframe(visible);
        std::cout << "[Tessellation] Triangulation: " << (visible ? "ON" : "OFF") << std::endl;
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_LEFT) {
        app->m_Renderer->SetTessellationLevel(app->m_Renderer->GetTessellationLevel() - 1.0f);
        std::cout << "[Tessellation] Level: " << app->m_Renderer->GetTessellationLevel() << std::endl;
    }
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && key == GLFW_KEY_RIGHT) {
        app->m_Renderer->SetTessellationLevel(app->m_Renderer->GetTessellationLevel() + 1.0f);
        std::cout << "[Tessellation] Level: " << app->m_Renderer->GetTessellationLevel() << std::endl;
    }

}
