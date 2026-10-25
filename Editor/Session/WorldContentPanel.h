// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/WorldContentSession.h"
#include "Editor/Session/EditorSelection.h"
enum class WorldContentActionKind { None,CreateRegion,Load,Unload,Save,Edit,DiscardHistory,BindShared };
struct WorldContentAction
{
    WorldContentActionKind kind=WorldContentActionKind::None;
    std::string region,name;
    cy::Vec3f minimum{0.0f},maximum{100.0f};
    ModelId model;
    MaterialHandle material,replacement;
};
// Drawn inside the Scene window; operations are deferred until before the next PrepareFrame.
class WorldContentPanel
{
public:
    WorldContentAction Draw(WorldContentSession&,SceneState&,const Renderer&,const SceneAssetRoots&,const EditorSelection&);
private:
    char m_Name[129]="Region";
    cy::Vec3f m_Minimum{0.0f},m_Maximum{100.0f};
    std::string m_ClearRegion;
};
