// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include "Editor/EditorSelection.h"

// Application 持有此状态，Inspector 与 Material Editor 共用；只保存身份，不持有资源。
class EditorMaterialSelection
{
public:
    bool AllMaterials() const { return m_AllMaterials; }
    ModelId Owner() const { return m_Owner; }
    MaterialHandle Material() const { return m_Material; }

    void SetAllMaterials(bool enabled)
    {
        if (m_AllMaterials == enabled)
            return;
        m_AllMaterials = enabled;
        m_Initialized = false;
        m_Owner = {};
        m_Material = {};
    }

    void Select(ModelId owner, MaterialHandle material)
    {
        m_AllMaterials = false;
        m_Initialized = true;
        m_Owner = owner;
        m_Material = material;
    }

    void SelectGlobal(MaterialHandle material) { m_Material = material; }

    std::vector<MaterialHandle> Synchronize(
        const EditorSelection& selection,
        const std::vector<EditableModel>& models,
        const std::vector<MaterialHandle>& liveMaterials)
    {
        ModelId owner;
        std::vector<MaterialHandle> candidates;
        if (m_AllMaterials)
            candidates = liveMaterials;
        else if (selection.type == EditorSelectionType::Model)
        {
            const EditableModel* model = FindEditableModel(models, selection.modelId);
            if (model)
            {
                owner = model->id;
                for (MaterialHandle handle : model->GetUsedMaterials())
                {
                    if (Contains(liveMaterials, handle))
                        candidates.push_back(handle);
                }
            }
        }

        // 进入新模型或显式切换范围时才自动选择首项；名称相同不代表同一资源。
        if (!m_Initialized || owner != m_Owner)
        {
            m_Material = candidates.empty() ? MaterialHandle{} : candidates.front();
        }
        else if (!Contains(candidates, m_Material))
        {
            // 同一范围内资源失效时清空，禁止因列表缩短而悄悄编辑另一材质。
            m_Material = {};
        }
        m_Owner = owner;
        m_Initialized = true;
        return candidates;
    }

private:
    static bool Contains(const std::vector<MaterialHandle>& handles, MaterialHandle wanted)
    {
        return wanted.IsValid() && std::any_of(handles.begin(), handles.end(),
            [wanted](MaterialHandle handle) { return handle.id == wanted.id; });
    }

    bool m_AllMaterials = false;
    bool m_Initialized = false;
    ModelId m_Owner;
    MaterialHandle m_Material;
};
