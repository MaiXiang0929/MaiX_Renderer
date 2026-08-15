// SPDX-License-Identifier: MIT
/// @file Renderer.cpp
/// @brief 渲染器类的实现文件
/// @details 该文件实现了 Renderer 类的核心功能，包括初始化渲染管线、设置场景网格和材质、加载立方体贴图、执行渲染管线等。
/// @author MaiX
/// @date 2026-08-11


#include "Renderer.h"

#include <algorithm>
#include <iostream>
#include <utility>

#include "Renderer/View/RenderView.h"

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

bool Renderer::Init()
{
    glEnable(GL_DEPTH_TEST);

    if (!m_RenderPipeline.Init())
    {
        std::cerr << "[Renderer] RenderPipeline initialization failed."
                  << std::endl;
        return false;
    }

    // Present Pass 使用的屏幕四边形，只负责显示 ForwardPass 颜色纹理。
    const std::vector<Vertex> presentVertices = {
        {{-1,-1,0},{0,0,1},{0,0}}, {{ 1,-1,0},{0,0,1},{1,0}}, {{ 1, 1,0},{0,0,1},{1,1}},
        {{-1,-1,0},{0,0,1},{0,0}}, {{ 1, 1,0},{0,0,1},{1,1}}, {{-1, 1,0},{0,0,1},{0,1}}
    };
    m_PresentMesh.Upload(presentVertices);

    const float s = 1.0f;
    const std::vector<Vertex> skyboxVertices = {
        {{ s,-s,-s},{0,0,0},{0,0}}, {{ s, s,-s},{0,0,0},{0,0}}, {{ s, s, s},{0,0,0},{0,0}},
        {{ s,-s,-s},{0,0,0},{0,0}}, {{ s, s, s},{0,0,0},{0,0}}, {{ s,-s, s},{0,0,0},{0,0}},
        {{-s,-s, s},{0,0,0},{0,0}}, {{-s, s, s},{0,0,0},{0,0}}, {{-s, s,-s},{0,0,0},{0,0}},
        {{-s,-s, s},{0,0,0},{0,0}}, {{-s, s,-s},{0,0,0},{0,0}}, {{-s,-s,-s},{0,0,0},{0,0}},
        {{ s, s,-s},{0,0,0},{0,0}}, {{-s, s,-s},{0,0,0},{0,0}}, {{-s, s, s},{0,0,0},{0,0}},
        {{ s, s,-s},{0,0,0},{0,0}}, {{-s, s, s},{0,0,0},{0,0}}, {{ s, s, s},{0,0,0},{0,0}},
        {{ s,-s, s},{0,0,0},{0,0}}, {{-s,-s, s},{0,0,0},{0,0}}, {{-s,-s,-s},{0,0,0},{0,0}},
        {{ s,-s, s},{0,0,0},{0,0}}, {{-s,-s,-s},{0,0,0},{0,0}}, {{ s,-s,-s},{0,0,0},{0,0}},
        {{ s,-s, s},{0,0,0},{0,0}}, {{ s, s, s},{0,0,0},{0,0}}, {{-s, s, s},{0,0,0},{0,0}},
        {{ s,-s, s},{0,0,0},{0,0}}, {{-s, s, s},{0,0,0},{0,0}}, {{-s,-s, s},{0,0,0},{0,0}},
        {{-s,-s,-s},{0,0,0},{0,0}}, {{-s, s,-s},{0,0,0},{0,0}}, {{ s, s,-s},{0,0,0},{0,0}},
        {{-s,-s,-s},{0,0,0},{0,0}}, {{ s, s,-s},{0,0,0},{0,0}}, {{ s,-s,-s},{0,0,0},{0,0}}
    };
    m_SkyboxMesh.Upload(skyboxVertices);

    const std::vector<Vertex> groundVertices = {
        {{-1,0,-1},{0,1,0},{0,0}}, {{ 1,0,-1},{0,1,0},{0,0}}, {{ 1,0, 1},{0,1,0},{0,0}},
        {{-1,0,-1},{0,1,0},{0,0}}, {{ 1,0, 1},{0,1,0},{0,0}}, {{-1,0, 1},{0,1,0},{0,0}}
    };
    m_GroundMesh.Upload(groundVertices);

    LoadCubemap("assets/models/cubemap");
    return true;
}

MeshHandle Renderer::CreateMesh(const std::vector<Vertex>& vertices)
{
    if (m_MeshResources.size() >= InvalidRenderResourceId)
    {
        std::cerr << "[Renderer] Mesh resource handle space exhausted."
                  << std::endl;
        return {};
    }

    auto resource = std::make_unique<Mesh>();
    resource->Upload(vertices);
    const MeshHandle handle{
        static_cast<RenderResourceId>(m_MeshResources.size())
    };
    m_MeshResources.push_back(std::move(resource));
    return handle;
}

MaterialHandle Renderer::CreateMaterial(Material material)
{
    if (m_MaterialResources.size() >= InvalidRenderResourceId)
    {
        std::cerr << "[Renderer] Material resource handle space exhausted."
                  << std::endl;
        return {};
    }

    const MaterialHandle handle{
        static_cast<RenderResourceId>(m_MaterialResources.size())
    };
    m_MaterialResources.push_back(
        std::make_unique<Material>(std::move(material)));
    return handle;
}

bool Renderer::GetMaterialSnapshot(
    MaterialHandle handle,
    MaterialSnapshot& snapshot) const
{
    if (!handle.IsValid() || handle.id >= m_MaterialResources.size() ||
        !m_MaterialResources[handle.id])
        return false;

    const Material& material = *m_MaterialResources[handle.id];
    snapshot.handle = handle;
    snapshot.properties = material.GetProperties();
    snapshot.blendMode = material.GetBlendMode();
    for (std::size_t index = 0; index < MaterialTextureSlotCount; ++index)
    {
        snapshot.hasTextures[index] = static_cast<bool>(material.GetTexture(
            static_cast<MaterialTextureSlot>(index)));
    }
    return true;
}

MaterialHandle Renderer::GetMaterialHandle(std::size_t index) const
{
    if (index >= m_MaterialResources.size())
        return {};
    return MaterialHandle{ static_cast<RenderResourceId>(index) };
}

bool Renderer::UpdateMaterial(
    MaterialHandle handle,
    const MaterialProperties& properties,
    BlendMode blendMode)
{
    if (!handle.IsValid() || handle.id >= m_MaterialResources.size() ||
        !m_MaterialResources[handle.id])
        return false;

    MaterialProperties clamped = properties;
    clamped.metallic = std::clamp(clamped.metallic, 0.0f, 1.0f);
    clamped.roughness = std::clamp(clamped.roughness, 0.045f, 1.0f);
    clamped.ambientOcclusion = std::clamp(clamped.ambientOcclusion, 0.0f, 1.0f);
    clamped.normalScale = std::clamp(clamped.normalScale, 0.0f, 4.0f);
    clamped.opacity = std::clamp(clamped.opacity, 0.0f, 1.0f);
    clamped.baseColor.x = std::clamp(clamped.baseColor.x, 0.0f, 1.0f);
    clamped.baseColor.y = std::clamp(clamped.baseColor.y, 0.0f, 1.0f);
    clamped.baseColor.z = std::clamp(clamped.baseColor.z, 0.0f, 1.0f);

    Material& material = *m_MaterialResources[handle.id];
    material.GetProperties() = clamped;
    material.SetBlendMode(blendMode);
    m_RenderScene.UpdateMaterialBlendMode(handle.id, blendMode);
    return true;
}

PrimitiveId Renderer::AddPrimitive(
    MeshHandle mesh,
    MaterialHandle material,
    const cy::Matrix4f& localToWorld,
    PrimitiveBounds bounds,
    bool castsShadow)
{
    if (!mesh.IsValid() || !material.IsValid() ||
        mesh.id >= m_MeshResources.size() ||
        material.id >= m_MaterialResources.size())
    {
        std::cerr << "[Renderer] AddPrimitive rejected invalid resource handle."
                  << std::endl;
        return InvalidPrimitiveId;
    }

    PrimitiveSceneProxy proxy;
    proxy.mesh = m_MeshResources[mesh.id].get();
    proxy.material = m_MaterialResources[material.id].get();
    proxy.shaderId = DefaultSurfaceShaderId;
    proxy.materialId = material.id;
    proxy.meshId = mesh.id;
    proxy.localToWorld = localToWorld;
    proxy.localBounds = bounds;
    proxy.castsShadow = castsShadow;
    proxy.blendMode = proxy.material->GetBlendMode();

    return m_RenderScene.AddPrimitive(proxy);
}

PrimitiveId Renderer::AddPrimitive(
    const std::vector<Vertex>& vertices,
    Material material,
    const cy::Matrix4f& localToWorld,
    PrimitiveBounds bounds,
    bool castsShadow)
{
    const MeshHandle mesh = CreateMesh(vertices);
    const MaterialHandle materialHandle = CreateMaterial(std::move(material));
    return AddPrimitive(
        mesh,
        materialHandle,
        localToWorld,
        bounds,
        castsShadow);
}

bool Renderer::UpdatePrimitiveTransform(
    PrimitiveId id,
    const cy::Matrix4f& localToWorld)
{
    return m_RenderScene.UpdatePrimitiveTransform(id, localToWorld);
}

LightId Renderer::AddLight(LightSceneProxy light)
{
    return m_RenderScene.AddLight(light);
}

bool Renderer::UpdateLight(LightId id, const LightSceneProxy& light)
{
    return m_RenderScene.UpdateLight(id, light);
}

void Renderer::LoadCubemap(const std::string& directoryPath)
{
    if (m_Cubemap.Load(directoryPath))
    {
        std::cout << "[Renderer] Cubemap loaded from: "
                  << directoryPath << std::endl;
    }
}

void Renderer::ExecutePipeline(RenderFrameData& frame)
{
    frame.shadowsEnabled = m_ShadowsEnabled;
    frame.editorPrimitivesEnabled = m_EditorPrimitivesEnabled;

    RenderView mainView;
    mainView.type = RenderViewType::Main;
    mainView.view = frame.view;
    mainView.projection = frame.projection;
    mainView.viewProjection = frame.projection * frame.view;
    mainView.frustum = Frustum::FromViewProjection(mainView.viewProjection);
    mainView.cameraWorldPosition = frame.cameraWorldPosition;
    mainView.viewportWidth = frame.viewportWidth;
    mainView.viewportHeight = frame.viewportHeight;
    m_RenderScene.BuildRenderView(mainView);

    // 主视图构建完成后固化只读快照，编辑器无需访问 RenderScene 或 Pass 私有容器。
    m_StatisticsSnapshot = {
        mainView.sourcePrimitiveCount,
        mainView.visiblePrimitiveCount,
        mainView.culledPrimitiveCount,
        mainView.opaqueDrawCount,
        mainView.opaqueBatchCount,
        mainView.translucentItems.size(),
        mainView.opaqueShaderGroupCount,
        mainView.opaqueMaterialGroupCount,
        mainView.opaqueMeshGroupCount,
        m_MeshResources.size(),
        m_MaterialResources.size()
    };
    LogMainViewStatsIfChanged(mainView);

    RenderView reflectionView;
    reflectionView.type = RenderViewType::Reflection;
    reflectionView.view = frame.reflectionView;
    reflectionView.projection = frame.projection;
    reflectionView.viewProjection = frame.projection * frame.reflectionView;
    reflectionView.frustum = Frustum::FromViewProjection(
        reflectionView.viewProjection);
    reflectionView.cameraWorldPosition = frame.cameraWorldPosition;
    reflectionView.viewportWidth = frame.viewportWidth;
    reflectionView.viewportHeight = frame.viewportHeight;
    m_RenderScene.BuildRenderView(reflectionView);

    RenderView shadowView;
    shadowView.type = RenderViewType::Shadow;
    shadowView.viewProjection = frame.lightVP;
    shadowView.frustum = Frustum::FromViewProjection(frame.lightVP);
    m_RenderScene.BuildRenderView(shadowView);

    // Renderer 在这里完成资源注入，Application 和 RenderPipeline 都不直接拥有资源。
    RenderPassContext context{
        frame,
        mainView,
        reflectionView,
        shadowView,
        m_PresentMesh,
        m_SkyboxMesh,
        m_GroundMesh,
        m_Cubemap,
        m_Tessellation,
        m_PostProcess
    };
    m_RenderPipeline.Execute(context);
}

void Renderer::LogMainViewStatsIfChanged(const RenderView& view)
{
    const std::array<std::size_t, 10> stats = {
        view.sourcePrimitiveCount,
        view.visiblePrimitiveCount,
        view.culledPrimitiveCount,
        view.opaqueDrawCount,
        view.opaqueBatchCount,
        view.opaqueShaderGroupCount,
        view.opaqueMaterialGroupCount,
        view.opaqueMeshGroupCount,
        m_MeshResources.size(),
        m_MaterialResources.size()
    };
    if (m_HasMainViewStats && stats == m_LastMainViewStats)
        return;

    m_LastMainViewStats = stats;
    m_HasMainViewStats = true;
    std::cout
        << "[RenderView][Main] source=" << stats[0]
        << " visible=" << stats[1]
        << " culled=" << stats[2]
        << " opaqueDraws=" << stats[3]
        << " opaqueBatches=" << stats[4]
        << " shaderGroups=" << stats[5]
        << " materialGroups=" << stats[6]
        << " meshGroups=" << stats[7]
        << " meshResources=" << stats[8]
        << " materialResources=" << stats[9]
        << std::endl;
}

void Renderer::ReloadShaders()
{
    if (m_RenderPipeline.ReloadShaders())
        std::cout << "[Renderer] Shaders reloaded successfully!" << std::endl;
    else
        std::cerr << "[Renderer] One or more shaders failed to reload."
                  << std::endl;
}

void Renderer::SetTessellationLevel(float level)
{
    m_Tessellation.level = std::clamp(level, 1.0f, 64.0f);
}

void Renderer::SetDisplacementScale(float scale)
{
    m_Tessellation.displacementScale = std::max(scale, 0.0f);
}

void Renderer::SetExposureCompensation(float exposure)
{
    m_PostProcess.exposureCompensation = ClampExposureCompensation(exposure);
}
