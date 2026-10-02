// SPDX-License-Identifier: MIT
/// @file Renderer.cpp
/// @brief 渲染器类的实现文件
/// @details 该文件实现了 Renderer 类的核心功能，包括初始化渲染管线、设置场景网格和材质、加载立方体贴图、执行渲染管线等。
/// @author MaiX
/// @date 2026-08-11


#include "Renderer.h"

#include <algorithm>
#include <cmath>
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

MeshHandle Renderer::CreateMesh(
    const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    if (m_MeshResources.size() >= InvalidRenderResourceId)
    {
        std::cerr << "[Renderer] Mesh resource handle space exhausted."
                  << std::endl;
        return {};
    }

    auto resource = std::make_unique<Mesh>();
    resource->Upload(vertices, indices);
    const MeshHandle handle{
        static_cast<RenderResourceId>(m_MeshResources.size())
    };
    m_MeshResources.push_back(std::move(resource));
    return handle;
}

bool Renderer::DestroyMesh(MeshHandle handle)
{
    if (!handle.IsValid() || handle.id >= m_MeshResources.size() ||
        !m_MeshResources[handle.id])
        return false;

    if (m_RenderScene.HasMeshReference(handle.id))
    {
        std::cerr << "[Renderer] DestroyMesh rejected: mesh " << handle.id
                  << " is still referenced by a primitive." << std::endl;
        return false;
    }

    m_MeshResources[handle.id].reset();
    return true;
}

MaterialHandle Renderer::CreateMaterial(Material material)
{
    return m_MaterialResources.Create(std::move(material));
}
MaterialHandle Renderer::CreateMaterialInstance(MaterialHandle parent, std::string name)
{
    return m_MaterialResources.CreateInstance(parent, std::move(name));
}
bool Renderer::DestroyMaterial(MaterialHandle handle)
{
    if (m_RenderScene.HasMaterialReference(handle.id)) return false;
    return m_MaterialResources.Destroy(handle);
}
bool Renderer::IsMaterialInstance(MaterialHandle handle) const
{
    const auto* entry = m_MaterialResources.Get(handle);
    return entry && entry->instance.has_value();
}
bool Renderer::GetMaterialSnapshot(MaterialHandle handle, MaterialSnapshot& snapshot) const
{
    const auto* entry = m_MaterialResources.Get(handle);
    if (!entry) return false;
    snapshot = {};
    const Material& material = entry->effective;
    snapshot.handle = handle;
    snapshot.name = material.GetName();
    snapshot.properties = material.GetProperties();
    snapshot.blendMode = material.GetBlendMode();
    snapshot.isInstance = entry->instance.has_value();
    if (entry->instance)
    {
        snapshot.parent = entry->instance->parent;
        snapshot.overrides = entry->instance->overrides;
        const auto* base = m_MaterialResources.Get(snapshot.parent);
        if (base) snapshot.parentName = base->effective.GetName();
    }
    for (std::size_t i = 0; i < MaterialTextureSlotCount; ++i)
    {
        const auto slot = static_cast<MaterialTextureSlot>(i);
        const auto& texture = material.GetTexture(slot);
        snapshot.hasTextures[i] = static_cast<bool>(texture);
        snapshot.textureIds[i] = texture ? texture->GetID() : 0;
        snapshot.textureSources[i] = material.GetTextureSource(slot);
        if (entry->instance) snapshot.textureModes[i] = entry->instance->textures[i].mode;
    }
    return true;
}
MaterialHandle Renderer::GetMaterialHandle(std::size_t index) const
{
    if (index >= InvalidRenderResourceId) return {};
    MaterialHandle handle{static_cast<RenderResourceId>(index)};
    return m_MaterialResources.Get(handle) ? handle : MaterialHandle{};
}
std::vector<MaterialHandle> Renderer::GetMaterialHandles() const
{
    return m_MaterialResources.Handles();
}
bool Renderer::UpdateMaterial(MaterialHandle handle, const MaterialProperties& properties, BlendMode blendMode)
{
    auto affected = m_MaterialResources.Handles();
    affected.erase(std::remove_if(affected.begin(), affected.end(), [&](MaterialHandle material) {
        const auto* entry = m_MaterialResources.Get(material);
        return material.id != handle.id && (!entry->instance || entry->instance->parent.id != handle.id);
    }), affected.end());
    if (!m_MaterialResources.UpdateBase(handle, properties, blendMode)) return false;
    // 跟踪列表在提交前分配完成；只同步父材质及其子实例，包含不可见 Primitive。
    for (const auto material : affected)
        m_RenderScene.UpdateMaterialBlendMode(material.id,
            m_MaterialResources.Get(material)->effective.GetBlendMode());
    return true;
}
bool Renderer::UpdateMaterialOverride(MaterialHandle handle, MaterialParameter parameter,
                                      const MaterialProperties& values, bool enabled)
{
    return m_MaterialResources.UpdateOverride(handle, parameter, values, enabled);
}
bool Renderer::ResetMaterialOverrides(MaterialHandle handle)
{
    return m_MaterialResources.ResetOverrides(handle);
}
bool Renderer::UpdateMaterialTexture(MaterialHandle handle, MaterialTextureSlot slot,
                                     std::shared_ptr<Texture2D> texture, std::string sourceLabel)
{
    return m_MaterialResources.UpdateTexture(handle, slot, std::move(texture), std::move(sourceLabel));
}
bool Renderer::ClearMaterialTexture(MaterialHandle handle, MaterialTextureSlot slot)
{
    return m_MaterialResources.ClearTexture(handle, slot);
}
bool Renderer::ResetMaterialTexture(MaterialHandle handle, MaterialTextureSlot slot)
{
    return m_MaterialResources.ResetTexture(handle, slot);
}
bool Renderer::ReplacePrimitiveMaterials(const std::vector<PrimitiveId>& primitives,
                                         MaterialHandle expected, MaterialHandle replacement)
{
    const auto* entry = m_MaterialResources.Get(replacement);
    if (!entry || !m_MaterialResources.Get(expected)) return false;
    return m_RenderScene.ReplacePrimitiveMaterials(primitives, expected.id, replacement.id,
        &entry->effective, entry->effective.GetBlendMode());
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
        !m_MaterialResources.Get(material) ||
        !m_MeshResources[mesh.id])
    {
        std::cerr << "[Renderer] AddPrimitive rejected invalid resource handle."
                  << std::endl;
        return InvalidPrimitiveId;
    }

    PrimitiveSceneProxy proxy;
    proxy.mesh = m_MeshResources[mesh.id].get();
    proxy.material = &m_MaterialResources.Get(material)->effective;
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
    if (!mesh.IsValid())
        return InvalidPrimitiveId;

    MaterialHandle materialHandle;
    try
    {
        materialHandle = CreateMaterial(std::move(material));
        if (materialHandle.IsValid())
        {
            const PrimitiveId id = AddPrimitive(
                mesh,
                materialHandle,
                localToWorld,
                bounds,
                castsShadow);
            if (id != InvalidPrimitiveId)
                return id;

            DestroyMaterial(materialHandle);
        }
    }
    catch (...)
    {
        if (materialHandle.IsValid())
            DestroyMaterial(materialHandle);
        DestroyMesh(mesh);
        throw;
    }

    DestroyMesh(mesh);
    return InvalidPrimitiveId;
}

bool Renderer::UpdatePrimitiveTransform(
    PrimitiveId id,
    const cy::Matrix4f& localToWorld)
{
    return m_RenderScene.UpdatePrimitiveTransform(id, localToWorld);
}

bool Renderer::RemovePrimitive(PrimitiveId id)
{
    return m_RenderScene.RemovePrimitive(id);
}

std::size_t Renderer::GetMeshResourceCount() const
{
    return static_cast<std::size_t>(std::count_if(
        m_MeshResources.begin(),
        m_MeshResources.end(),
        [](const std::unique_ptr<Mesh>& resource) { return resource != nullptr; }));
}

std::size_t Renderer::GetMaterialResourceCount() const
{
    return m_MaterialResources.Count();
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
    frame.shadowsEnabled = frame.shadowsEnabled && m_ShadowsEnabled && frame.shadowLightId != InvalidLightId;
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
        GetMeshResourceCount(),
        GetMaterialResourceCount()
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

    // Renderer 注入场景绘制资源；Pipeline 组织 Pass，并绑定各 Pass 自己拥有的渲染目标。
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
        GetMeshResourceCount(),
        GetMaterialResourceCount()
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

bool Renderer::ReloadShaders()
{
    const bool loaded = m_RenderPipeline.ReloadShaders();
    if (loaded)
        std::cout << "[Renderer] Shaders reloaded successfully!" << std::endl;
    else
        std::cerr << "[Renderer] One or more shaders failed to reload."
                  << std::endl;
    return loaded;
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

void Renderer::SetBloomThreshold(float threshold)
{
    m_PostProcess.bloomThreshold = ClampBloomThreshold(threshold);
}

void Renderer::SetBloomIntensity(float intensity)
{
    m_PostProcess.bloomIntensity = ClampBloomIntensity(intensity);
}

void Renderer::SetSsaoRadius(float radius)
{
    m_PostProcess.ssaoRadius = ClampSsaoRadius(radius);
}

void Renderer::SetSsaoIntensity(float intensity)
{
    m_PostProcess.ssaoIntensity = ClampSsaoIntensity(intensity);
}

void Renderer::SetSsaoBias(float bias)
{
    m_PostProcess.ssaoBias = ClampSsaoBias(bias);
}
