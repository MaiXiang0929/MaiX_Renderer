// SPDX-License-Identifier: MIT
/// @file Renderer.h
/// @brief 渲染器类的头文件
/// @details 该文件声明了 Renderer 类，它是渲染资源的所有者和渲染管线的门面，负责准备资源上下文并启动 RenderPipeline。
/// @author MaiX
/// @date 2026-08-11

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Renderer/Resources/CubemapTexture.h"
#include "Renderer/Diagnostics/RenderSubmissionStats.h"
#include "Renderer/Resources/Material.h"
#include "Renderer/Resources/Mesh.h"
#include "Renderer/Pipeline/RenderPipeline.h"
#include "Renderer/Pipeline/RenderSettings.h"
#include "Renderer/Scene/RenderScene.h"

/// @brief 渲染资源的所有者与管线门面。
/// @details Renderer 不再实现具体 Pass，只负责准备资源上下文并启动 RenderPipeline。
class Renderer
{
public:
    struct MaterialSnapshot
    {
        MaterialHandle handle;
        MaterialProperties properties;
        BlendMode blendMode = BlendMode::Opaque;
        std::array<bool, MaterialTextureSlotCount> hasTextures{};
    };

    struct StatisticsSnapshot
    {
        std::size_t sourcePrimitiveCount = 0;
        std::size_t visiblePrimitiveCount = 0;
        std::size_t culledPrimitiveCount = 0;
        std::size_t opaqueDrawCount = 0;
        std::size_t opaqueBatchCount = 0;
        std::size_t translucentDrawCount = 0;
        std::size_t shaderGroupCount = 0;
        std::size_t materialGroupCount = 0;
        std::size_t meshGroupCount = 0;
        std::size_t meshResourceCount = 0;
        std::size_t materialResourceCount = 0;
    };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Init();

    MeshHandle CreateMesh(const std::vector<Vertex>& vertices);
    MaterialHandle CreateMaterial(Material material);
    bool GetMaterialSnapshot(
        MaterialHandle handle,
        MaterialSnapshot& snapshot) const;
    MaterialHandle GetMaterialHandle(std::size_t index) const;
    bool UpdateMaterial(
        MaterialHandle handle,
        const MaterialProperties& properties,
        BlendMode blendMode);

    PrimitiveId AddPrimitive(
        MeshHandle mesh,
        MaterialHandle material,
        const cy::Matrix4f& localToWorld,
        PrimitiveBounds bounds = {},
        bool castsShadow = true);

    /// @brief 便利接口：创建独立 Mesh/Material 后提交一个 Primitive。
    PrimitiveId AddPrimitive(
        const std::vector<Vertex>& vertices,
        Material material,
        const cy::Matrix4f& localToWorld,
        PrimitiveBounds bounds = {},
        bool castsShadow = true);
    bool UpdatePrimitiveTransform(
        PrimitiveId id,
        const cy::Matrix4f& localToWorld);

    LightId AddLight(LightSceneProxy light);
    bool UpdateLight(LightId id, const LightSceneProxy& light);

    void LoadCubemap(const std::string& directoryPath);

    void ExecutePipeline(RenderFrameData& frame);
    void ReloadShaders();
    const GpuTimingSnapshot& GetGpuTimingSnapshot() const
    {
        return m_RenderPipeline.GetGpuTimingSnapshot();
    }
    const StatisticsSnapshot& GetStatisticsSnapshot() const
    {
        return m_StatisticsSnapshot;
    }
    RenderSubmissionSnapshot GetSubmissionSnapshot() const
    {
        return RenderSubmissionStats::Get().GetSnapshot();
    }

    std::size_t GetMeshResourceCount() const
    {
        return m_MeshResources.size();
    }
    std::size_t GetMaterialResourceCount() const
    {
        return m_MaterialResources.size();
    }

    void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }
    bool IsShadowsEnabled() const { return m_ShadowsEnabled; }

    void SetEditorPrimitivesEnabled(bool enabled)
    {
        m_EditorPrimitivesEnabled = enabled;
    }
    bool AreEditorPrimitivesEnabled() const
    {
        return m_EditorPrimitivesEnabled;
    }

    void SetTessellationEnabled(bool enabled)
    {
        m_Tessellation.enabled = enabled;
    }
    bool IsTessellationEnabled() const
    {
        return m_Tessellation.enabled;
    }
    void SetTessellationLevel(float level);
    float GetTessellationLevel() const { return m_Tessellation.level; }
    void SetDisplacementScale(float scale);
    float GetDisplacementScale() const
    {
        return m_Tessellation.displacementScale;
    }
    void SetTessellationWireframe(bool enabled)
    {
        m_Tessellation.wireframe = enabled;
    }
    bool IsTessellationWireframe() const
    {
        return m_Tessellation.wireframe;
    }

    void SetToneMappingEnabled(bool enabled)
    {
        m_PostProcess.toneMappingEnabled = enabled;
    }
    bool IsToneMappingEnabled() const
    {
        return m_PostProcess.toneMappingEnabled;
    }
    void SetExposureCompensation(float exposure);
    float GetExposureCompensation() const
    {
        return m_PostProcess.exposureCompensation;
    }

private:
    void LogMainViewStatsIfChanged(const RenderView& view);

    Mesh m_PresentMesh;
    Mesh m_SkyboxMesh;
    Mesh m_GroundMesh;

    // Renderer 唯一拥有场景 GPU 资源；Scene Proxy 只缓存解析后的非 owning 指针。
    std::vector<std::unique_ptr<Mesh>> m_MeshResources;
    std::vector<std::unique_ptr<Material>> m_MaterialResources;
    RenderScene m_RenderScene;
    CubemapTexture m_Cubemap;
    RenderPipeline m_RenderPipeline;
    TessellationSettings m_Tessellation;
    PostProcessSettings m_PostProcess;
    StatisticsSnapshot m_StatisticsSnapshot;
    std::array<std::size_t, 10> m_LastMainViewStats{};
    bool m_HasMainViewStats = false;
    bool m_ShadowsEnabled = true;
    bool m_EditorPrimitivesEnabled = true;
};
