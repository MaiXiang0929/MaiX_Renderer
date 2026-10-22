// SPDX-License-Identifier: MIT
#pragma once
#include <bitset>
#include "Engine/Rendering/Material/Material.h"
#include "Engine/Rendering/Renderer/Resources/RenderResourceHandle.h"

struct MaterialTextureOverride
{
    TextureOverrideMode mode = TextureOverrideMode::Inherit;
    std::shared_ptr<Texture2D> texture;
    std::string source;
};

// 单层实例只保存父句柄和覆盖项，不拥有 Shader，也不复制 GPU 纹理。
struct MaterialInstance
{
    MaterialHandle parent;
    MaterialProperties values;
    MaterialOverrideMask overrides;
    std::array<MaterialTextureOverride, MaterialTextureSlotCount> textures;

    Material Resolve(const Material& base, const std::string& name) const;
};
MaterialProperties SanitizeMaterialProperties(const MaterialProperties& properties);
