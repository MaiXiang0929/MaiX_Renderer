// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Renderer/Scene/LightSceneProxy.h"
#include "Renderer/Scene/PrimitiveSceneProxy.h"

struct RenderView;

class RenderScene
{
public:
    PrimitiveId AddPrimitive(PrimitiveSceneProxy proxy);
    bool RemovePrimitive(PrimitiveId id);
    bool HasMeshReference(RenderResourceId meshId) const;
    bool HasMaterialReference(RenderResourceId materialId) const;
    bool ReplacePrimitiveMaterials(const std::vector<PrimitiveId>& ids,
        RenderResourceId expected, RenderResourceId replacement, const Material* material,
        BlendMode blendMode);
    bool UpdatePrimitiveTransform(
        PrimitiveId id,
        const cy::Matrix4f& localToWorld);
    void UpdateMaterialBlendMode(
        RenderResourceId materialId,
        BlendMode blendMode);

    LightId AddLight(LightSceneProxy light);
    bool UpdateLight(LightId id, const LightSceneProxy& light);
    bool RemoveLight(LightId id);

    void BuildRenderView(RenderView& view) const;

private:
    std::vector<PrimitiveSceneProxy> m_Primitives;
    std::vector<LightSceneProxy> m_Lights;
    PrimitiveId m_NextPrimitiveId = 0;
    LightId m_NextLightId = 0;
};
