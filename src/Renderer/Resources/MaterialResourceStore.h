// SPDX-License-Identifier: MIT
#pragma once
#include <optional>
#include <vector>
#include "MaterialInstance.h"

struct MaterialResource
{
    Material effective;
    std::optional<MaterialInstance> instance;
};

// Renderer 拥有此表；unique_ptr 保证有效材质地址不因槽位追加而改变。
class MaterialResourceStore
{
public:
    ~MaterialResourceStore();
    MaterialHandle Create(Material material);
    MaterialHandle CreateInstance(MaterialHandle parent, std::string name = {});
    const MaterialResource* Get(MaterialHandle handle) const;
    bool Destroy(MaterialHandle handle);
    bool UpdateBase(MaterialHandle handle, const MaterialProperties& properties, BlendMode blend);
    bool UpdateOverride(MaterialHandle handle, MaterialParameter parameter,
                        const MaterialProperties& values, bool enabled = true);
    bool ResetOverrides(MaterialHandle handle);
    bool UpdateTexture(MaterialHandle handle, MaterialTextureSlot slot,
                       std::shared_ptr<Texture2D> texture, std::string source);
    bool ClearTexture(MaterialHandle handle, MaterialTextureSlot slot);
    bool ResetTexture(MaterialHandle handle, MaterialTextureSlot slot);
    std::vector<MaterialHandle> Handles() const;
    std::size_t Count() const;
private:
    bool CommitBase(MaterialHandle handle, Material material);
    bool CommitInstance(MaterialHandle handle, MaterialInstance instance);
    std::vector<std::unique_ptr<MaterialResource>> m_Resources;
};
