// SPDX-License-Identifier: MIT
/// @file Application.cpp
/// @brief 应用程序核心管理类的实现文件
/// @details 该文件实现了 Application 类的核心功能，包括初始化、主循环更新、渲染及资源清理。
/// @author MaiX
/// @date 2026-07-31

#include"Application.h"
#include "Editor/RendererStatisticsPanel.h"
#include "Renderer/Core/Renderer.h"
#include "InstanceGrid.h"
#include "CameraClipRange.h"

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

#include "Editor/AssetImportPanel.h"
#include "Assets/Import/FbxModelImporter.h"
#include "Assets/Import/ImageLoader.h"
#include "Editor/EditorViewportController.h"
#include "Editor/InspectorPanel.h"
#include "Editor/MaterialEditorPanel.h"
#include "Editor/SceneHierarchyPanel.h"
#include "Editor/EditorWorkspace.h"

#include "cyTriMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
// The bundled OBJ has no unit metadata. This is a project asset convention.
constexpr float BundledModelMetersPerUnit = 0.01f;
std::shared_ptr<Texture2D> CreateOrmCheckerTexture(
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
    return Texture2D::CreateRGBA8(
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
Application::Application(
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
Application::~Application() {
    Shutdown();
}

/// @brief 运行应用程序
void Application::Run() {
    // 初始化引擎，如果失败直接退出
    if (!Init()) {
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
bool Application::Init() {
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "[Error] Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Tessellation shaders require OpenGL 4.0 or newer.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 开启 OpenGL Debug Context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);    // MacOS 需要设置前向兼容
#endif

    // 创建编辑器主窗口，并使用对外展示的引擎名称作为标题。
    m_Window = glfwCreateWindow(m_Width, m_Height, "MaiX Engine", nullptr, nullptr);
    if (!m_Window) {
        std::cerr << "[Error] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

	glfwMakeContextCurrent(m_Window);   // 将窗口的上下文设置为当前线程的上下文

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth > 0 && framebufferHeight > 0) {
        m_Width = static_cast<unsigned int>(framebufferWidth);
        m_Height = static_cast<unsigned int>(framebufferHeight);
    }

    // 将当前的 Application 实例指针绑定到 GLFW 窗口上
    glfwSetWindowUserPointer(m_Window, this);

    // 绑定所有的输入回调
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_Window, CursorPositionCallback);
    glfwSetScrollCallback(m_Window, ScrollCallback);
    glfwSetKeyCallback(m_Window, KeyCallback);
    glfwSetCharCallback(m_Window, CharCallback);

    // 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[Error] Failed to initialize GLAD" << std::endl;  

        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
        glfwTerminate();

        return false;
    }

    // OpenGL 上下文已经可用，后续任一步失败都应由 Shutdown 统一释放资源。
    m_Initialized = true;

    // Application 保留 GLFW 回调所有权，ImGui 后端只接收手动转发的输入事件。
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
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
    m_StatisticsPanel = std::make_unique<RendererStatisticsPanel>();
    m_MaterialEditorPanel = std::make_unique<MaterialEditorPanel>();
    m_AssetImportPanel = std::make_unique<AssetImportPanel>();
    m_ViewportController = std::make_unique<EditorViewportController>();
    m_SceneHierarchyPanel = std::make_unique<SceneHierarchyPanel>();
    m_InspectorPanel = std::make_unique<InspectorPanel>();
    m_Workspace = std::make_unique<EditorWorkspace>();
    m_Workspace->AddLog("MaiX Renderer initialized.");

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
	m_Renderer = new Renderer();
	if (!m_Renderer->Init()) {
        std::cerr << "[Error] Renderer initialization failed." << std::endl;
        return false;
    }
    // ==========================================
    // 第一阶段妥协：在 App 层临时加载网格和纹理
    // ==========================================
    cy::TriMesh mesh;
    if (!mesh.LoadFromFileObj(m_ObjPath.c_str())) {
        std::cerr << "[Error] Failed to load OBJ: " << m_ObjPath << std::endl;
        return false;
    }

    mesh.ComputeBoundingBox();
    if (!mesh.HasNormals()) {
        // OBJ 未提供法线时由 cyTriMesh 自动生成，保证光照着色可用。
        mesh.ComputeNormals();
    }
    m_ObjCenter = (mesh.GetBoundMax() + mesh.GetBoundMin()) * 0.5f;

    // 根据包围盒自动调整观察距离，使不同尺寸的模型都能进入离屏画面。
    const float rawDiameter = (mesh.GetBoundMax() - mesh.GetBoundMin()).Length();
    const float modelDiameter = rawDiameter * BundledModelMetersPerUnit;
    m_ModelDiameter = modelDiameter;

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>(mesh.NF()) * 3);
    bool hasTexCoords = mesh.HasTextureVertices();
    for (int i = 0; i < mesh.NF(); ++i) {
        const cy::TriMesh::TriFace face = mesh.F(i);
        const cy::TriMesh::TriFace faceNormal = mesh.FN(i);
        const cy::TriMesh::TriFace faceTex = hasTexCoords
            ? mesh.FT(i)
            : cy::TriMesh::TriFace();

        cy::Vec3f positions[3];
        cy::Vec3f normals[3];
        cy::Vec2f uvs[3];
        for (int j = 0; j < 3; ++j) {
            positions[j] = mesh.V(face.v[j]);
            normals[j] = mesh.VN(faceNormal.v[j]);
            if (hasTexCoords) {
                const cy::Vec3f uv = mesh.VT(faceTex.v[j]);
                uvs[j] = cy::Vec2f(uv.x, 1.0f - uv.y);
            }
            else {
                uvs[j] = cy::Vec2f(0.0f, 0.0f);
            }
        }

        const cy::Vec3f edge1 = positions[1] - positions[0];
        const cy::Vec3f edge2 = positions[2] - positions[0];
        const cy::Vec2f deltaUv1 = uvs[1] - uvs[0];
        const cy::Vec2f deltaUv2 = uvs[2] - uvs[0];
        const float determinant =
            deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;

        cy::Vec3f faceTangent(1.0f, 0.0f, 0.0f);
        cy::Vec3f faceBitangent(0.0f, 0.0f, 1.0f);
        if (hasTexCoords && std::abs(determinant) > 1.0e-8f) {
            const float inverseDeterminant = 1.0f / determinant;
            faceTangent =
                (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * inverseDeterminant;
            faceBitangent =
                (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * inverseDeterminant;
        }

        for (int j = 0; j < 3; ++j) {
            cy::Vec3f normal = normals[j];
            const float normalLength = normal.Length();
            if (normalLength > 1.0e-8f)
                normal /= normalLength;

            const float tangentDotNormal =
                faceTangent.x * normal.x +
                faceTangent.y * normal.y +
                faceTangent.z * normal.z;
            cy::Vec3f tangent = faceTangent - normal * tangentDotNormal;
            if (tangent.Length() <= 1.0e-8f) {
                tangent = std::abs(normal.y) < 0.999f
                    ? cy::Vec3f(normal.z, 0.0f, -normal.x)
                    : cy::Vec3f(1.0f, 0.0f, 0.0f);
            }
            tangent.Normalize();

            const cy::Vec3f crossNormalTangent(
                normal.y * tangent.z - normal.z * tangent.y,
                normal.z * tangent.x - normal.x * tangent.z,
                normal.x * tangent.y - normal.y * tangent.x);
            const float handednessDot =
                crossNormalTangent.x * faceBitangent.x +
                crossNormalTangent.y * faceBitangent.y +
                crossNormalTangent.z * faceBitangent.z;

            Vertex vertex{};
            vertex.Position = positions[j];
            vertex.Normal = normals[j];
            vertex.TexCoord = uvs[j];
            vertex.Tangent = cy::Vec4f(
                tangent.x, tangent.y, tangent.z,
                handednessDot < 0.0f ? -1.0f : 1.0f);
            vertices.push_back(vertex);
        }
    }
    std::filesystem::path diffusePath;
    std::filesystem::path specularPath;
    Material mainMaterial;
    mainMaterial.SetName("Surface");
    const std::filesystem::path modelDirectory =
        std::filesystem::path(m_ObjPath).parent_path();

    // 当前渲染器使用一组材质纹理，因此选取 MTL 中首个有效的漫反射和高光贴图。
    // 贴图路径以 OBJ 所在目录为基准进行解析。
    for (unsigned int i = 0; i < mesh.NM(); ++i) {
        const cy::TriMesh::Mtl& material = mesh.M(i);
        if (i == 0) {
            MaterialProperties& properties = mainMaterial.GetProperties();
            properties.baseColor = cy::Vec3f(
                material.Kd[0], material.Kd[1], material.Kd[2]);
            properties.specularColor = cy::Vec3f(
                material.Ks[0], material.Ks[1], material.Ks[2]);
            properties.shininess = std::max(material.Ns, 1.0f);
            properties.environmentReflectivity = std::max({
                material.Ks[0], material.Ks[1], material.Ks[2] });
        }
        if (diffusePath.empty() && material.map_Kd.data != nullptr) {
            diffusePath = modelDirectory / material.map_Kd.data;
        }
        if (specularPath.empty() && material.map_Ks.data != nullptr) {
            specularPath = modelDirectory / material.map_Ks.data;
        }
    }

    if (!diffusePath.empty()) {
        mainMaterial.SetAlbedoMap(Texture2D::Load(
            diffusePath.lexically_normal().string(), TextureColorSpace::SRGB));
    }
    if (!specularPath.empty()) {
        mainMaterial.SetSpecularMap(Texture2D::Load(
            specularPath.lexically_normal().string(), TextureColorSpace::Linear));
    }
    if (!m_NormalMapPath.empty()) {
        mainMaterial.SetNormalMap(Texture2D::Load(
            m_NormalMapPath, TextureColorSpace::Linear));
    }
    if (!m_DisplacementMapPath.empty()) {
        mainMaterial.SetDisplacementMap(Texture2D::Load(
            m_DisplacementMapPath, TextureColorSpace::Linear));
    }
    PrimitiveBounds bounds;
    bounds.center = m_ObjCenter;
    bounds.radius = rawDiameter * 0.5f;
    const std::uint32_t effectiveGridSize = (m_MaterialLab || m_MaterialInstanceLab)
        ? 2
        : (m_InstanceGridSize == 0 ? 1 : m_InstanceGridSize);
    const float instanceSpacing = std::max(rawDiameter * 1.25f, 0.001f);
    const std::vector<cy::Vec3f> instanceOffsets = BuildInstanceGridOffsets(
        effectiveGridSize, instanceSpacing);
    m_SceneRadius = CalculateInstanceGridSceneRadius(
        effectiveGridSize, instanceSpacing, bounds.radius) * BundledModelMetersPerUnit;
    const float gridHalfSpan =
        static_cast<float>(effectiveGridSize - 1) * instanceSpacing * 0.5f;
    m_DefaultBoundsMin = mesh.GetBoundMin() - m_ObjCenter - cy::Vec3f(gridHalfSpan, gridHalfSpan, 0.0f);
    m_DefaultBoundsMax = mesh.GetBoundMax() - m_ObjCenter + cy::Vec3f(gridHalfSpan, gridHalfSpan, 0.0f);
    m_GroundY = (mesh.GetBoundMin().y - m_ObjCenter.y - gridHalfSpan -
        rawDiameter * 0.02f) * BundledModelMetersPerUnit;
    const float cameraDistance = modelDiameter > 0.0f
        ? std::max(modelDiameter * 1.25f, m_SceneRadius * 2.5f)
        : 5.0f;
    m_Camera.SetDistance(cameraDistance);
    m_Camera.SetClipPlanes(
        0.1f,
        std::max(1000.0f, cameraDistance + m_SceneRadius * 1.5f));

    m_Models.emplace_back();
    EditableModel& defaultModel = m_Models.back();
    defaultModel.transform.scale = cy::Vec3f(BundledModelMetersPerUnit);
    defaultModel.usesBundledCentimeterScale = true;
    const cy::Matrix4f defaultRoot = defaultModel.transform.ToMatrix();
    mainMaterial.GetProperties().outlineThickness *= BundledModelMetersPerUnit;
    m_Renderer->SetSsaoRadius(0.005f);
    m_Renderer->SetSsaoBias(0.00025f);
    defaultModel.id = ModelId{m_NextModelId++};
    m_DefaultModelId = defaultModel.id;
    defaultModel.name = m_MaterialInstanceLab ? "Material Instance Lab" :
        (m_MaterialLab ? "Material Lab" : "Teapot");

    const MeshHandle instanceMesh = m_Renderer->CreateMesh(vertices);
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
                cy::Matrix4f::Translation(-m_ObjCenter);
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
            CreateOrmCheckerTexture(42, 86, 255);
        const std::shared_ptr<Texture2D> roughMetalOrm =
            CreateOrmCheckerTexture(190, 235, 255);
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
                cy::Matrix4f::Translation(-m_ObjCenter);
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
                cy::Matrix4f::Translation(-m_ObjCenter);
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
            << " sceneRadius=" << m_SceneRadius
            << std::endl;
    }

    if (m_TranslucencyTest)
        CreateTranslucencyTestScene();

    // Adding translucent models may have reallocated m_Models. Resolve by ID.
    const PrimitiveBounds sceneBounds = FindEditableModel(m_Models, m_DefaultModelId)->GetWorldBounds();
    const cy::Vec3f sceneCenter = sceneBounds.radius > 0.0f
        ? sceneBounds.center
        : cy::Vec3f(0.0f);
    const float lightDistance = std::max(
        std::sqrt(500.0f) * BundledModelMetersPerUnit,
        std::max(sceneBounds.radius, BundledModelMetersPerUnit) * 2.5f);
    LightSceneProxy mainLight;
    mainLight.type = LightType::Spot;
    mainLight.position = sceneCenter + cy::Vec3f(
        0.0f,
        lightDistance / std::sqrt(5.0f),
        lightDistance * 2.0f / std::sqrt(5.0f));
    mainLight.direction = sceneCenter - mainLight.position;
    mainLight.direction.Normalize();
    mainLight.color = cy::Vec3f(1.0f, 0.95f, 0.85f);
    mainLight.intensity = 5.0f;
    mainLight.range = 30.0f * BundledModelMetersPerUnit;
    mainLight.outerConeAngle = 30.0f * 3.14159265358979323846f / 180.0f;
    m_MainLightId = m_Renderer->AddLight(mainLight);
    mainLight.id = m_MainLightId;
    EditableLight editableMainLight;
    editableMainLight.name = "Main Spot Light";
    editableMainLight.transform.position = mainLight.position;
    editableMainLight.proxy = mainLight;
    m_EditableLights.push_back(editableMainLight);

    LightSceneProxy directionalFill;
    directionalFill.type = LightType::Directional;
    directionalFill.direction = cy::Vec3f(-0.35f, -1.0f, -0.2f);
    directionalFill.direction.Normalize();
    directionalFill.color = cy::Vec3f(0.35f, 0.5f, 1.0f);
    directionalFill.intensity = 0.35f;
    directionalFill.castsShadow = false;
    directionalFill.id = m_Renderer->AddLight(directionalFill);
    EditableLight editableDirectionalFill;
    editableDirectionalFill.name = "Directional Fill Light";
    editableDirectionalFill.proxy = directionalFill;
    m_EditableLights.push_back(editableDirectionalFill);

    LightSceneProxy pointFill;
    pointFill.type = LightType::Point;
    pointFill.position = cy::Vec3f(-8.0f, 4.0f, 5.0f) * BundledModelMetersPerUnit;
    pointFill.color = cy::Vec3f(1.0f, 0.2f, 0.08f);
    pointFill.intensity = 0.9f;
    pointFill.range = 18.0f * BundledModelMetersPerUnit;
    pointFill.castsShadow = false;
    pointFill.id = m_Renderer->AddLight(pointFill);
    EditableLight editablePointFill;
    editablePointFill.name = "Point Fill Light";
    editablePointFill.transform.position = pointFill.position;
    editablePointFill.proxy = pointFill;
    m_EditableLights.push_back(editablePointFill);

    if (!LoadStartupFaceShadowDemo())
        return false;

    return true;
}

void Application::CreateTranslucencyTestScene()
{
    if (m_Renderer == nullptr || m_ModelDiameter <= 0.0f)
        return;

    const float halfWidth = m_ModelDiameter * 0.24f;
    const float halfHeight = m_ModelDiameter * 0.38f;
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
            cy::Vec3f(-m_ModelDiameter * 0.16f, 0.0f, -m_ModelDiameter * 0.28f)},
        {cy::Vec3f(0.95f, 0.10f, 0.55f), 0.42f,
            cy::Vec3f(0.0f, 0.0f, 0.0f)},
        {cy::Vec3f(0.95f, 0.78f, 0.08f), 0.58f,
            cy::Vec3f(m_ModelDiameter * 0.16f, 0.0f, m_ModelDiameter * 0.28f)}
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
    const std::shared_ptr<Texture2D> alphaTexture = Texture2D::CreateRGBA8(
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
bool Application::CommitImportedModel(
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

    EditableModel pendingModel;
    pendingModel.name = model.name;
    pendingModel.importedSizeMeters = model.boundsMax - model.boundsMin;
    pendingModel.sourceUnitMeters = model.sourceUnitMeters;
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
        std::vector<std::shared_ptr<Texture2D>> textures(
            model.textures.size());
        for (std::size_t index = 0; index < model.textures.size(); ++index)
        {
            const AssetImport::ImportedTextureData& importedTexture =
                model.textures[index];
            if (!importedTexture.image.IsValid())
            {
                return fail(
                    "Imported texture data is invalid: " +
                    importedTexture.name);
            }

            textures[index] = Texture2D::CreateRGBA8(
                importedTexture.image.width,
                importedTexture.image.height,
                importedTexture.image.pixels,
                TextureColorSpace::SRGB);
            if (!textures[index])
            {
                return fail(
                    "GPU texture creation failed: " +
                    importedTexture.name);
            }
        }

        for (const AssetImport::ImportedMaterialData& importedMaterial :
            model.materials)
        {
            Material material;
            material.SetName(importedMaterial.name);
            MaterialProperties& properties = material.GetProperties();
            properties.baseColor = importedMaterial.baseColor;
            properties.metallic = importedMaterial.metallic;
            properties.roughness = importedMaterial.roughness;
            properties.opacity = importedMaterial.opacity;
            if (importedMaterial.opacity < 0.999f)
                material.SetBlendMode(BlendMode::AlphaBlend);

            if (importedMaterial.baseColorTexture !=
                AssetImport::InvalidImportedTextureIndex)
            {
                if (importedMaterial.baseColorTexture >= textures.size())
                {
                    return fail(
                        "Imported material references an invalid texture: " +
                        importedMaterial.name);
                }

                const AssetImport::ImportedTextureData& importedTexture =
                    model.textures[importedMaterial.baseColorTexture];
                const std::string sourceLabel = importedTexture.embedded
                    ? "Embedded: " + importedTexture.name
                    : importedTexture.sourcePath.u8string();
                if (!material.SetTexture(
                        MaterialTextureSlot::BaseColor,
                        textures[importedMaterial.baseColorTexture],
                        sourceLabel))
                {
                    return fail(
                        "Unable to bind the base color texture for material: " +
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
            EditableModelSection section{
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

    if (m_NextModelId == 0)
        return fail("Scene model ID space exhausted.");
    pendingModel.id = ModelId{m_NextModelId};
    try
    {
        pendingModel.name = MakeUniqueModelName(m_Models, model.name);
        if (pendingModel.name.empty())
            return fail("Scene model name space exhausted.");
        m_Models.push_back(std::move(pendingModel));
    }
    catch (const std::exception& exception)
    {
        return fail(std::string("Unable to add the model to the scene: ") +
            exception.what());
    }
    importedId = ModelId{m_NextModelId++};

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

bool Application::LoadStartupFaceShadowDemo()
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
    for (std::size_t index = 0; index < m_Models.size();)
    {
        if (m_Models[index].id == importedId)
            ++index;
        else
            RemoveModel(m_Models[index].id);
    }
    EditableModel* demoModel = FindEditableModel(m_Models, importedId);
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
        m_ObjCenter = demoBounds.center;
        m_ModelDiameter = demoBounds.radius * 2.0f;
        m_SceneRadius = demoBounds.radius;
        m_GroundY = demoBounds.center.y - demoBounds.radius;
        m_Camera.FocusBounds(demoBounds.center, demoBounds.radius);
        m_PendingFocusSelection.SelectModel(importedId);

        EditableLight* mainLight = FindEditableLight(m_MainLightId);
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
            ApplyEditableLightTransform(*mainLight, *m_Renderer);
        }
    }

    const std::shared_ptr<Texture2D> faceShadowTexture =
        Texture2D::CreateRGBA8(
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

void Application::DestroyModelResources(EditableModel& model)
{
    if (m_Renderer == nullptr)
    {
        model.sections.clear();
        model.meshes.clear();
        model.materials.clear();
        return;
    }

    for (const EditableModelSection& section : model.sections)
        m_Renderer->RemovePrimitive(section.primitiveId);
    for (const MeshHandle mesh : model.meshes)
        m_Renderer->DestroyMesh(mesh);
    // 模型可能先记录父材质后记录实例；按依赖顺序销毁，不能依赖数组顺序。
    for (int instances = 1; instances >= 0; --instances)
        for (const MaterialHandle material : model.materials)
        {
            if (m_Renderer->IsMaterialInstance(material) == (instances != 0))
                m_Renderer->DestroyMaterial(material);
        }
    model.sections.clear();
    model.meshes.clear();
    model.materials.clear();
}

void Application::ApplyMaterialEditRequest(const MaterialEditRequest& request)
{
    if (request.action == MaterialEditAction::None) return;
    EditableModel* model = FindEditableModel(m_Models, request.model);
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

bool Application::RemoveModel(ModelId id)
{
    const auto found = std::find_if(m_Models.begin(), m_Models.end(),
        [id](const EditableModel& model) { return model.id == id; });
    if (found == m_Models.end())
        return false;
    if (m_EditorSelection.IsModelSelected(id))
        m_EditorSelection.Clear();
    if (m_DefaultModelId == id)
        m_DefaultModelId = {};
    DestroyModelResources(*found);
    m_Models.erase(found);
    return true;
}

EditableLight* Application::FindEditableLight(std::uint32_t id)
{
    const auto iterator = std::find_if(
        m_EditableLights.begin(), m_EditableLights.end(),
        [id](const EditableLight& light) { return light.proxy.id == id; });
    return iterator == m_EditableLights.end() ? nullptr : &*iterator;
}

const EditableLight* Application::FindEditableLight(std::uint32_t id) const
{
    const auto iterator = std::find_if(
        m_EditableLights.begin(), m_EditableLights.end(),
        [id](const EditableLight& light) { return light.proxy.id == id; });
    return iterator == m_EditableLights.end() ? nullptr : &*iterator;
}

void Application::Update() {
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
        m_PendingFocusSelection = m_EditorSelection;
        m_AssetImportPanel->ReportCommitSuccess(
            completedImport->model,
            FindEditableModel(m_Models, importedId)->sections.size());
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
void Application::Render() {
    // 先建立 ImGui 布局，取得中央视口的实际像素尺寸，再提交渲染帧。
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    void* nativeWindowHandle = GetNativeWindowHandle(m_Window);
    m_Workspace->BeginFrame(*m_AssetImportPanel, nativeWindowHandle);
    const EditorViewportRegion viewport = m_Workspace->BeginViewport();
    if (viewport.visible && viewport.pixelWidth > 0 && viewport.pixelHeight > 0)
    {
        m_Camera.SetAspectRatio(static_cast<float>(viewport.pixelWidth) /
            static_cast<float>(viewport.pixelHeight));
        if (m_PendingFocusSelection.type != EditorSelectionType::None)
        {
            m_ViewportController->FocusSelection(m_Camera, m_PendingFocusSelection,
                m_Models, m_EditableLights);
            m_PendingFocusSelection.Clear();
        }
    }

    // 共用矩阵
    cy::Matrix4f projMatrix = m_Camera.GetProjectionMatrix();
    cy::Matrix4f viewMatrix = m_Camera.GetViewMatrix();

    const PrimitiveBounds sceneBounds = GetSceneWorldBounds(m_Models);
    const cy::Vec3f sceneCenter = sceneBounds.radius > 0.0f
        ? sceneBounds.center
        : cy::Vec3f(0.0f, 0.0f, 0.0f);
    const float sceneRadius = std::max(
        std::max(m_SceneRadius, sceneBounds.radius), 0.001f);

    EditableLight* editableMainLight = FindEditableLight(m_MainLightId);
    const cy::Vec3f lightWorldPosition = editableMainLight
        ? editableMainLight->transform.position
        : sceneCenter + cy::Vec3f(0.0f, 0.1f, 0.2f);
    const cy::Vec3f lightOffsetWorld = lightWorldPosition - sceneCenter;
    const float rawLightDistance = lightOffsetWorld.Length();
    const float lightDistance = std::max(rawLightDistance, 0.001f);
    const cy::Vec3f lightViewTarget = rawLightDistance > 1.0e-4f
        ? sceneCenter
        : lightWorldPosition + (editableMainLight
            ? editableMainLight->proxy.direction
            : cy::Vec3f(0.0f, -1.0f, 0.0f));

    // 聚光灯始终朝向模型中心；位置仍由 Ctrl + 左键独立于相机旋转。
    // 当光线方向接近竖直方向时切换 up 向量，避免 View 矩阵基向量退化。
    const cy::Vec3f lightViewDirection = lightViewTarget - lightWorldPosition;
    const bool nearVertical =
        std::abs(lightViewDirection.x) + std::abs(lightViewDirection.z) < 0.001f;
    const cy::Vec3f lightUp = nearVertical
        ? cy::Vec3f(0.0f, 0.0f, 1.0f)
        : cy::Vec3f(0.0f, 1.0f, 0.0f);
    const cy::Matrix4f lightView = cy::Matrix4f::View(
        lightWorldPosition, lightViewTarget, lightUp);

    // 根据模型包围球自动拟合聚光灯锥体，兼容不同尺寸的命令行 OBJ。
    constexpr float Pi = 3.14159265358979323846f;
    const float shadowLightDistance = lightDistance;
    const float angularRadius = std::asin(
        std::min(sceneRadius / shadowLightDistance, 0.95f));
    const float lightFov = std::clamp(
        angularRadius * 2.0f + 10.0f * Pi / 180.0f,
        60.0f * Pi / 180.0f,
        150.0f * Pi / 180.0f);
    const float lightNear = std::max(
        0.001f, shadowLightDistance - sceneRadius * 1.25f);
    const float lightFar = shadowLightDistance + sceneRadius * 2.0f;
    const cy::Matrix4f lightProjection = cy::Matrix4f::Perspective(
        lightFov, 1.0f, lightNear, lightFar);
    const cy::Matrix4f lightVP = lightProjection * lightView;
    // 反射视图仍由 Application 根据场景地面位置计算，Pass 只消费结果。
    float groundY = m_GroundY;
    if (m_Models.size() == 1 && m_Models.front().id == m_DefaultModelId)
    {
        const EditableModel& model = m_Models.front();
        const cy::Matrix4f root = model.transform.ToMatrix();
        groundY = std::numeric_limits<float>::max();
        for (float x : {m_DefaultBoundsMin.x, m_DefaultBoundsMax.x})
            for (float y : {m_DefaultBoundsMin.y, m_DefaultBoundsMax.y})
                for (float z : {m_DefaultBoundsMin.z, m_DefaultBoundsMax.z})
                {
                    const cy::Vec4f corner = root * cy::Vec4f(x, y, z, 1.0f);
                    groundY = std::min(groundY, corner.y);
                }
        const float maxScale = std::max({std::abs(model.transform.scale.x),
            std::abs(model.transform.scale.y), std::abs(model.transform.scale.z)});
        groundY -= (m_ModelDiameter / BundledModelMetersPerUnit) * 0.02f * maxScale;
    }
    else
    {
        bool hasModelBounds = false;
        for (const EditableModel& model : m_Models)
        {
            const PrimitiveBounds bounds = model.GetWorldBounds();
            if (bounds.radius <= 0.0f)
                continue;
            const float bottom = bounds.center.y - bounds.radius;
            groundY = hasModelBounds ? std::min(groundY, bottom) : bottom;
            hasModelBounds = true;
        }
    }
    const cy::Matrix4f reflectMatrix =
        cy::Matrix4f::Translation(cy::Vec3f(0.0f, groundY, 0.0f)) *
        cy::Matrix4f::Scale(1.0f, -1.0f, 1.0f) *
        cy::Matrix4f::Translation(cy::Vec3f(0.0f, -groundY, 0.0f));
    const cy::Matrix4f reflectView = viewMatrix * reflectMatrix;

    // 相机世界位置（用于地面着色器视线方向计算）
    const cy::Vec3f cameraWorldPos = m_Camera.GetPosition();

    const float groundSize = std::max(
        m_ModelDiameter * 2.0f, sceneRadius * 2.0f);
    const cy::Vec3f groundCenter(sceneCenter.x, groundY, sceneCenter.z);
    const cy::Matrix4f groundModel =
        cy::Matrix4f::Translation(groundCenter) *
        cy::Matrix4f::Scale(groundSize, 1.0f, groundSize);

    if (editableMainLight)
    {
        const cy::Vec3f direction = sceneCenter - lightWorldPosition;
        if (direction.Length() > 1.0e-6f)
            editableMainLight->proxy.direction = direction.GetNormalized();
        ApplyEditableLightTransform(*editableMainLight, *m_Renderer);
    }

    // Main and reflection share a projection. Fit both depth ranges before
    // creating it, including the ground quad and the editor's light cones.
    CameraClipRange clipRange;
    for (const EditableModel& model : m_Models)
    {
        const cy::Matrix4f root = model.transform.ToMatrix();
        for (const EditableModelSection& section : model.sections)
        {
            if (section.localBounds.radius <= 0.0f)
                continue;
            PrimitiveBounds localBounds = section.localBounds;
            if (m_Renderer->IsTessellationEnabled())
                localBounds.radius += m_Renderer->GetDisplacementScale();
            const PrimitiveBounds bounds = TransformBounds(localBounds, root * section.localTransform);
            clipRange.IncludeSphere(viewMatrix, bounds.center, bounds.radius);
            clipRange.IncludeSphere(reflectView, bounds.center, bounds.radius);
        }
    }
    for (float x : {-1.0f, 1.0f})
        for (float z : {-1.0f, 1.0f})
            clipRange.IncludeSphere(viewMatrix,
                groundCenter + cy::Vec3f(x * groundSize, 0.0f, z * groundSize), 0.0f);
    for (const EditableLight& light : m_EditableLights)
    {
        if (light.proxy.type == LightType::Directional)
            continue;
        clipRange.IncludeSphere(viewMatrix, light.transform.position, 0.0f);
        if (light.proxy.type == LightType::Spot)
        {
            const float range = light.proxy.range;
            const float coneRadius = range * std::tan(light.proxy.outerConeAngle);
            const float radius = std::sqrt(range * range * 0.25f + coneRadius * coneRadius);
            clipRange.IncludeSphere(viewMatrix, light.transform.position +
                light.proxy.direction * (range * 0.5f), radius);
        }
    }
    m_Camera.SetClipPlanes(clipRange.Near(), clipRange.Far());
    projMatrix = m_Camera.GetProjectionMatrix();

    // Application 只提交强类型帧数据，不再创建任何 Pass callback。
    RenderFrameData frame;
    frame.viewportWidth = viewport.pixelWidth;
    frame.viewportHeight = viewport.pixelHeight;
    frame.projection = projMatrix;
    frame.view = viewMatrix;
    frame.lightVP = lightVP;
    frame.shadowLightId = m_MainLightId;
    frame.keyLightId = m_MainLightId;
    frame.reflectionView = reflectView;
    frame.groundModel = groundModel;
    frame.groundMvp = projMatrix * viewMatrix * groundModel;
    frame.reflectionVP = projMatrix * reflectView;
    frame.cameraWorldPosition = cameraWorldPos;
    if (viewport.visible && m_Width > 0 && m_Height > 0)
        m_Renderer->ExecutePipeline(frame);

    // GPU 最终纹理由 Renderer 持有；编辑器在同一帧将其显示到中央面板。
    m_Workspace->DrawViewportImage(m_Renderer->GetFinalColorTexture());
    if (viewport.visible)
        m_ViewportController->Draw(
            m_Camera, m_EditorSelection, m_Models,
            m_EditableLights, *m_Renderer, viewport);
    m_Workspace->EndViewport();

    m_AssetImportPanel->Draw(nativeWindowHandle);
    if (m_Workspace->ShowStatistics())
        m_StatisticsPanel->Draw(*m_Renderer);
    const ModelId removedModel = m_SceneHierarchyPanel->Draw(
        m_EditorSelection, m_Models, m_EditableLights);
    if (removedModel.IsValid())
        RemoveModel(removedModel);
    // 先应用场景选择/删除，再同步材质身份；即使材质面板关闭也清理失效选择。
    m_MaterialSelection.Synchronize(
        m_EditorSelection, m_Models, m_Renderer->GetMaterialHandles());
    if (m_InspectorPanel->Draw(
            m_EditorSelection, m_Models, m_EditableLights, *m_Renderer,
            m_MaterialSelection))
        m_Workspace->OpenMaterialEditor();
    if (m_Workspace->ShowMaterialEditor())
    {
        // NextWindow 设置必须紧邻材质面板 Begin，避免被其它窗口消费。
        m_Workspace->PrepareMaterialEditor();
        MaterialEditRequest request;
        m_MaterialEditorPanel->Draw(
            *m_Renderer, m_EditorSelection, m_Models, m_MaterialSelection, request,
            nativeWindowHandle);
        ApplyMaterialEditRequest(request);
    }
    m_Workspace->DrawBottomPanels(*m_AssetImportPanel, nativeWindowHandle);
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
    glClearColor(0.09f, 0.09f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}
/// @brief 关闭应用程序
void Application::Shutdown() {
	
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

	// 释放渲染器资源
	if (m_Renderer) {
        for (EditableModel& model : m_Models)
            DestroyModelResources(model);
        m_Models.clear();
		delete m_Renderer;
		m_Renderer = nullptr;
	}

	// 释放 GLFW 窗口资源
    if (m_Window) {
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
	}

	// 终止 GLFW
	glfwTerminate();

    m_Initialized = false;

	std::cout << "[System] Engine Shutdown Successfully!" << std::endl;
}

// ==========================================
// 静态回调函数实现（使用 UserPointer 访问实例）
// ==========================================
void Application::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->m_Width = width;
        app->m_Height = height;

        // 相机投影宽高比由中央 Viewport 决定，而不是整个 GLFW 窗口。
        glViewport(0, 0, width, height);
    }
}

void Application::MouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
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

void Application::CursorPositionCallback(
    GLFWwindow* window,
    double xpos,
    double ypos)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
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
        altDown && (inViewport || scenePointerActive),
        viewport.size.y,
        app->m_Camera,
        &deltaX,
        &deltaY);

    if (!inViewport && !scenePointerActive)
        return;
    if (!altDown && ctrlDown && app->m_ViewportController->IsLeftDown())
    {
        EditableLight* mainLight = app->FindEditableLight(app->m_MainLightId);
        if (mainLight)
        {
            const PrimitiveBounds bounds = GetSceneWorldBounds(app->m_Models);
            const cy::Vec3f center = bounds.radius > 0.0f
                ? bounds.center
                : cy::Vec3f(0.0f);
            const cy::Vec3f offset = mainLight->transform.position - center;
            const cy::Matrix4f rotation =
                cy::Matrix4f::RotationX(deltaY * 0.01f) *
                cy::Matrix4f::RotationY(deltaX * 0.01f);
            const cy::Vec4f rotated = rotation *
                cy::Vec4f(offset.x, offset.y, offset.z, 0.0f);
            mainLight->transform.position = center +
                cy::Vec3f(rotated.x, rotated.y, rotated.z);
        }
    }
}

void Application::ScrollCallback(
    GLFWwindow* window,
    double xoffset,
    double yoffset)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app || !app->m_ViewportController || !app->m_Workspace)
        return;
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    const EditorViewportRegion& viewport = app->m_Workspace->GetViewportRegion();
    if (!viewport.hovered || !viewport.Contains(x, y))
        return;
    app->m_ViewportController->ProcessScroll(
        static_cast<float>(yoffset), app->m_Camera);
}

void Application::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_CharCallback(window, codepoint);
}

void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
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
            app->m_PendingFocusSelection = app->m_EditorSelection;
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
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
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
        std::cout << "[Renderer] Teapot Tessellation: "
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
