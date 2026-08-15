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

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "Editor/MaterialEditorPanel.h"

#include "cyTriMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
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
}

/// @brief 构造函数
Application::Application(
    std::string normalMapPath,
    std::string displacementMapPath,
    std::uint32_t instanceGridSize,
    bool materialLab)
    :
    m_Camera(
        cy::Vec3f(0, 0, 0),
        50.0f
    ),
    m_PlaneCamera(
        cy::Vec3f(0, 0, 0),
        3.5f
    ),
    m_NormalMapPath(std::move(normalMapPath)),
    m_DisplacementMapPath(std::move(displacementMapPath)),
    m_InstanceGridSize(instanceGridSize),
    m_MaterialLab(materialLab)
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

    // 创建窗口 (默认 1920x1080)
    m_Window = glfwCreateWindow(m_Width, m_Height, "OpenGL Engine", nullptr, nullptr);
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

    // 主颜色目标和显示平面都跟随窗口 framebuffer 的像素宽高比。
    const float framebufferAspect =
        static_cast<float>(m_Width) / static_cast<float>(m_Height);
    m_Camera.SetAspectRatio(framebufferAspect);
    m_PlaneCamera.SetAspectRatio(framebufferAspect);

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
    const float modelDiameter = (mesh.GetBoundMax() - mesh.GetBoundMin()).Length();
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
    bounds.radius = m_ModelDiameter * 0.5f;
    const std::uint32_t effectiveGridSize = m_MaterialLab
        ? 2
        : (m_InstanceGridSize == 0 ? 1 : m_InstanceGridSize);
    const float instanceSpacing = std::max(m_ModelDiameter * 1.25f, 0.001f);
    const std::vector<cy::Vec3f> instanceOffsets = BuildInstanceGridOffsets(
        effectiveGridSize, instanceSpacing);
    m_SceneRadius = CalculateInstanceGridSceneRadius(
        effectiveGridSize, instanceSpacing, bounds.radius);
    const float gridHalfSpan =
        static_cast<float>(effectiveGridSize - 1) * instanceSpacing * 0.5f;
    m_GroundY = mesh.GetBoundMin().y - m_ObjCenter.y - gridHalfSpan -
        modelDiameter * 0.02f;
    const float cameraDistance = modelDiameter > 0.0f
        ? std::max(modelDiameter * 1.25f, m_SceneRadius * 2.5f)
        : 5.0f;
    m_Camera.SetDistance(cameraDistance);
    m_Camera.SetClipPlanes(
        0.1f,
        std::max(1000.0f, cameraDistance + m_SceneRadius * 1.5f));

    const MeshHandle instanceMesh = m_Renderer->CreateMesh(vertices);
    if (!instanceMesh.IsValid())
    {
        std::cerr << "[Error] Failed to create the shared surface mesh."
                  << std::endl;
        return false;
    }
    MaterialHandle instanceMaterial;

    if (m_MaterialLab)
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

            const cy::Matrix4f localToWorld =
                cy::Matrix4f::Translation(instanceOffsets[index]) *
                cy::Matrix4f::Translation(-m_ObjCenter);
            const PrimitiveId primitiveId = m_Renderer->AddPrimitive(
                instanceMesh,
                materialHandle,
                localToWorld,
                bounds);
            if (m_MainPrimitiveId == InvalidPrimitiveId)
                m_MainPrimitiveId = primitiveId;
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

        for (const cy::Vec3f& offset : instanceOffsets)
        {
            const cy::Matrix4f localToWorld =
                cy::Matrix4f::Translation(offset) *
                cy::Matrix4f::Translation(-m_ObjCenter);
            const PrimitiveId primitiveId = m_Renderer->AddPrimitive(
                instanceMesh,
                instanceMaterial,
                localToWorld,
                bounds);
            if (m_MainPrimitiveId == InvalidPrimitiveId)
                m_MainPrimitiveId = primitiveId;
        }
    }
    if (m_MainPrimitiveId == InvalidPrimitiveId)
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

    CreateTranslucencyTestScene();

    LightSceneProxy mainLight;
    mainLight.type = LightType::Spot;
    mainLight.position = cy::Vec3f(0.0f, 10.0f, 20.0f);
    mainLight.direction = cy::Vec3f(0.0f, 0.0f, 0.0f) - mainLight.position;
    mainLight.direction.Normalize();
    mainLight.color = cy::Vec3f(1.0f, 0.95f, 0.85f);
    mainLight.intensity = 5.0f;
    mainLight.range = 30.0f;
    mainLight.outerConeAngle = 30.0f * 3.14159265358979323846f / 180.0f;
    m_MainLightId = m_Renderer->AddLight(mainLight);

    LightSceneProxy directionalFill;
    directionalFill.type = LightType::Directional;
    directionalFill.direction = cy::Vec3f(-0.35f, -1.0f, -0.2f);
    directionalFill.direction.Normalize();
    directionalFill.color = cy::Vec3f(0.35f, 0.5f, 1.0f);
    directionalFill.intensity = 0.35f;
    directionalFill.castsShadow = false;
    m_Renderer->AddLight(directionalFill);

    LightSceneProxy pointFill;
    pointFill.type = LightType::Point;
    pointFill.position = cy::Vec3f(-8.0f, 4.0f, 5.0f);
    pointFill.color = cy::Vec3f(1.0f, 0.2f, 0.08f);
    pointFill.intensity = 0.9f;
    pointFill.range = 18.0f;
    pointFill.castsShadow = false;
    m_Renderer->AddLight(pointFill);

    glfwGetCursorPos(m_Window, &m_LastX, &m_LastY);

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
void Application::Update() {

}

/// @brief 渲染应用程序
void Application::Render() {
    // 共用矩阵
    cy::Matrix4f projMatrix = m_Camera.GetProjectionMatrix();
    cy::Matrix4f viewMatrix = m_Camera.GetViewMatrix();
    cy::Matrix4f modelMatrix = cy::Matrix4f::Translation(-m_ObjCenter);

    // 光源世界位置
    const float lightDistance = std::max(
        std::sqrt(500.0f), m_SceneRadius * 2.5f);
    const cy::Vec3f lightBasePos(
        0.0f,
        lightDistance / std::sqrt(5.0f),
        lightDistance * 2.0f / std::sqrt(5.0f));
    const cy::Matrix4f lightRotation =
        cy::Matrix4f::RotationX(m_LightRotX) *
        cy::Matrix4f::RotationY(m_LightRotY);
    const cy::Vec4f lightPosWorld = lightRotation *
        cy::Vec4f(lightBasePos.x, lightBasePos.y, lightBasePos.z, 1.0f);

    const cy::Vec3f lightWorldPosition(
        lightPosWorld.x, lightPosWorld.y, lightPosWorld.z);

    // 聚光灯始终朝向模型中心；位置仍由 Ctrl + 左键独立于相机旋转。
    // 当光线方向接近竖直方向时切换 up 向量，避免 View 矩阵基向量退化。
    const bool nearVertical =
        std::abs(lightWorldPosition.x) + std::abs(lightWorldPosition.z) < 0.001f;
    const cy::Vec3f lightUp = nearVertical
        ? cy::Vec3f(0.0f, 0.0f, 1.0f)
        : cy::Vec3f(0.0f, 1.0f, 0.0f);
    const cy::Matrix4f lightView = cy::Matrix4f::View(
        lightWorldPosition, cy::Vec3f(0.0f, 0.0f, 0.0f), lightUp);

    // 根据模型包围球自动拟合聚光灯锥体，兼容不同尺寸的命令行 OBJ。
    constexpr float Pi = 3.14159265358979323846f;
    const float sceneRadius = std::max(m_SceneRadius, 1.0f);
    const float shadowLightDistance =
        std::max(lightWorldPosition.Length(), 0.001f);
    const float angularRadius = std::asin(
        std::min(sceneRadius / shadowLightDistance, 0.95f));
    const float lightFov = std::clamp(
        angularRadius * 2.0f + 10.0f * Pi / 180.0f,
        60.0f * Pi / 180.0f,
        150.0f * Pi / 180.0f);
    const float lightNear = std::max(
        0.1f, shadowLightDistance - sceneRadius * 1.25f);
    const float lightFar = shadowLightDistance + sceneRadius * 2.0f;
    const cy::Matrix4f lightProjection = cy::Matrix4f::Perspective(
        lightFov, 1.0f, lightNear, lightFar);
    const cy::Matrix4f lightVP = lightProjection * lightView;
    // 反射视图仍由 Application 根据场景地面位置计算，Pass 只消费结果。
    const cy::Matrix4f reflectMatrix =
        cy::Matrix4f::Translation(cy::Vec3f(0.0f, m_GroundY, 0.0f)) *
        cy::Matrix4f::Scale(1.0f, -1.0f, 1.0f) *
        cy::Matrix4f::Translation(cy::Vec3f(0.0f, -m_GroundY, 0.0f));
    const cy::Matrix4f reflectView = viewMatrix * reflectMatrix;

    // 相机世界位置（用于地面着色器视线方向计算）
    const float dist = m_Camera.GetPosition().z;
    const cy::Vec3f cameraWorldPos(
        viewMatrix.cell[2] * dist,
        viewMatrix.cell[6] * dist,
        viewMatrix.cell[10] * dist
    );

    const float groundSize = std::max(
        m_ModelDiameter * 2.0f, m_SceneRadius * 2.0f);
    const cy::Matrix4f groundModel =
        cy::Matrix4f::Translation(cy::Vec3f(
            -m_ObjCenter.x, m_GroundY, -m_ObjCenter.z)) *
        cy::Matrix4f::Scale(groundSize, 1.0f, groundSize);
    if (m_InstanceGridSize == 0)
        m_Renderer->UpdatePrimitiveTransform(m_MainPrimitiveId, modelMatrix);

    LightSceneProxy mainLight;
    mainLight.type = LightType::Spot;
    mainLight.position = lightWorldPosition;
    mainLight.direction = cy::Vec3f(0.0f, 0.0f, 0.0f) - lightWorldPosition;
    mainLight.direction.Normalize();
    mainLight.color = cy::Vec3f(1.0f, 0.95f, 0.85f);
    mainLight.intensity = 5.0f;
    mainLight.range = 30.0f;
    mainLight.outerConeAngle = 30.0f * 3.14159265358979323846f / 180.0f;
    m_Renderer->UpdateLight(m_MainLightId, mainLight);

    // Application 只提交强类型帧数据，不再创建任何 Pass callback。
    RenderFrameData frame;
    frame.viewportWidth = m_Width;
    frame.viewportHeight = m_Height;
    frame.projection = projMatrix;
    frame.view = viewMatrix;
    frame.lightVP = lightVP;
    frame.shadowLightId = m_MainLightId;
    frame.reflectionView = reflectView;
    frame.groundModel = groundModel;
    frame.groundMvp = projMatrix * viewMatrix * groundModel;
    frame.reflectionVP = projMatrix * reflectView;
    frame.cameraWorldPosition = cameraWorldPos;
    const float framebufferAspect = m_Height > 0
        ? static_cast<float>(m_Width) / static_cast<float>(m_Height)
        : 1.0f;
    frame.presentMvp =
        m_PlaneCamera.GetProjectionMatrix() *
        m_PlaneCamera.GetViewMatrix() *
        cy::Matrix4f::Scale(framebufferAspect, 1.0f, 1.0f);
    m_Renderer->ExecutePipeline(frame);

    // PresentPass 完成后在默认帧缓冲绘制编辑器 UI，避免污染场景颜色目标。
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    m_StatisticsPanel->Draw(*m_Renderer);
    m_MaterialEditorPanel->Draw(*m_Renderer);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}
/// @brief 关闭应用程序
void Application::Shutdown() {
	
    if (!m_Initialized) return;

    m_StatisticsPanel.reset();
    m_MaterialEditorPanel.reset();
    if (m_ImGuiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ImGuiInitialized = false;
    }

	// 释放渲染器资源
	if (m_Renderer) {
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

        if (width > 0 && height > 0) {
            const float aspect =
                static_cast<float>(width) / static_cast<float>(height);
            app->m_Camera.SetAspectRatio(aspect);
            app->m_PlaneCamera.SetAspectRatio(aspect);
        }

        glViewport(0, 0, width, height);
    }
}

void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
        app->m_LeftDown = false;
        app->m_RightDown = false;
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) app->m_LeftDown = true;
        else if (action == GLFW_RELEASE) app->m_LeftDown = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) app->m_RightDown = true;
        else if (action == GLFW_RELEASE) app->m_RightDown = false;
    }
}

void Application::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    double dx = xpos - app->m_LastX;
    double dy = ypos - app->m_LastY;
    app->m_LastX = xpos;
    app->m_LastY = ypos;

    // 即使 UI 捕获鼠标也更新上一帧位置，离开面板后相机不会发生跳变。
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
        return;

    const bool altDown =
        glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    app->m_CtrlDown =
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    // Alt 的优先级最高：按住 Alt 时始终操作纹理平面相机。
    Camera& activeCamera = altDown ? app->m_PlaneCamera : app->m_Camera;

    // 左键默认旋转相机；未按 Alt 而按住 Ctrl 时恢复原有的光源旋转操作。
    if (app->m_LeftDown) {
        if (!altDown && app->m_CtrlDown) {
            app->m_LightRotX += static_cast<float>(dx) * 0.01f;
            app->m_LightRotY += static_cast<float>(dy) * 0.01f;
        }
        else {
            activeCamera.ProcessMouseOrbit(static_cast<float>(dx), static_cast<float>(dy));
        }
    }

    // 右键缩放
    if (app->m_RightDown) {
        activeCamera.ProcessMouseZoom(static_cast<float>(dy));
    }
}

void Application::ScrollCallback(
    GLFWwindow* window,
    double xoffset,
    double yoffset)
{
    if (ImGui::GetCurrentContext())
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
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
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard)
        return;
    // 着色器重载（F6）
    if (key == GLFW_KEY_F6 && action == GLFW_PRESS) {
		app->m_Renderer->ReloadShaders();
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
