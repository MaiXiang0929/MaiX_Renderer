// SPDX-License-Identifier: MIT
#pragma once

#include <glad/glad.h>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Renderer/Scene/LightSceneProxy.h"
#include "PassResourceContract.h"

class CubemapTexture;
class Mesh;
struct RenderView;
struct TessellationSettings;
struct PostProcessSettings;

/// @brief Application 每帧提交给渲染器的纯场景数据。
/// @details 这里只保存矩阵、光源和视口等 CPU 数据，不持有任何 OpenGL 资源。
struct RenderFrameData
{
    unsigned int viewportWidth = 0;
    unsigned int viewportHeight = 0;

    cy::Matrix4f projection;
    cy::Matrix4f view;
    cy::Matrix4f lightVP;
    LightId shadowLightId = InvalidLightId;
    LightId keyLightId = InvalidLightId;

    cy::Matrix4f reflectionView;

    cy::Matrix4f groundMvp;
    cy::Matrix4f groundModel;
    cy::Matrix4f reflectionVP;
    cy::Vec3f cameraWorldPosition;

    bool shadowsEnabled = true;
    bool editorPrimitivesEnabled = true;
};

/// @brief RenderPipeline 在各 Pass 之间共享的执行上下文。
/// @details Renderer 填入场景引用；Pipeline 注入当前 Pass 的受限资源访问器，Pass 按契约读写附件。
struct RenderPassContext
{
    RenderFrameData& frame;

    RenderView& mainView;
    RenderView& reflectionView;
    RenderView& shadowView;
    Mesh& presentMesh;
    Mesh& skyboxMesh;
    Mesh& groundMesh;
    CubemapTexture& cubemap;
    TessellationSettings& tessellation;
    const PostProcessSettings& postProcess;

    // Pipeline 只在当前 Pass 执行期间注入受限访问器，不保留跨帧纹理字段。
    PassResources* passResources = nullptr;
    PassResources& Resources() const
    {
        if (!passResources)
            throw ResourceContractError("Pass resources are not active.");
        return *passResources;
    }
};

/// @brief 所有真实渲染阶段的统一接口。
class RenderPass
{
public:
    virtual ~RenderPass() = default;
    virtual RenderPassType GetType() const = 0;
    virtual void Execute(RenderPassContext& context) = 0;
};
