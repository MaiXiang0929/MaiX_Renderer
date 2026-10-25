// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/WorldContentDocument.h"
#include "Engine/Runtime/Engine/SceneState.h"

class Renderer;
struct ContentActorDescriptor
{
    std::string guid,region,name,parent;
    ActorKind kind=ActorKind::Empty;
};
struct ContentRegionStatus
{
    ContentRegionRecord record;
    bool loaded=false;
    std::string savedText,lastError;
};
// EngineSession owns this CPU-only directory. SceneState owns editable live values,
// Renderer owns GPU objects; this class owns neither a second World nor GPU resources.
class WorldContentSession
{
public:
    bool Ready() const { return !m_Document.id.empty(); }
    const std::string& Id() const { return m_Document.id; }
    const std::filesystem::path& File() const { return m_File; }
    const std::vector<ContentRegionStatus>& Regions() const { return m_Regions; }
    const std::vector<ContentActorDescriptor>& Catalog() const { return m_Catalog; }
    void Reset() noexcept;
    bool Create(const std::filesystem::path&, SceneState&, Renderer&, const SceneAssetRoots&, std::string& error);
    bool Open(const std::filesystem::path&, SceneState&, Renderer&, const SceneAssetRoots&, std::string& error);
    bool CreateRegion(const std::string& name,const cy::Vec3f& minimum,const cy::Vec3f& maximum,
        SceneState&,Renderer&,const SceneAssetRoots&,std::string& id,std::string& error);
    bool LoadRegion(const std::string&,SceneState&,Renderer&,const SceneAssetRoots&,std::string& error);
    bool UnloadRegion(const std::string&,SceneState&,Renderer&,const SceneAssetRoots&,std::string& error);
    bool SaveRegion(const std::string&,SceneState&,Renderer&,const SceneAssetRoots&,std::string& error);
    bool SaveAll(SceneState&,Renderer&,const SceneAssetRoots&,std::string& error);
    bool IsDirty(const std::string&,SceneState&,const Renderer&,const SceneAssetRoots&,std::string& error);
    bool HasUnsavedChanges(SceneState&,const Renderer&,const SceneAssetRoots&,std::string& error);
    bool SetEditingRegion(const std::string&,SceneState&,std::string& error) const;
    bool RefreshCatalog(const SceneState&,std::string& error);
    bool TrackMaterials(SceneState&,const Renderer&,const std::vector<MaterialHandle>&,
        const std::string& region,std::string& error) const;
    bool CanBindMaterial(const SceneState&,ActorId,MaterialHandle) const;
private:
    WorldContentDocument m_Document;
    std::filesystem::path m_File;
    std::vector<ContentRegionStatus> m_Regions;
    std::vector<ContentActorDescriptor> m_Catalog;
    std::string m_SavedWorld;
    bool Capture(const std::string&,SceneState&,const Renderer&,const SceneAssetRoots&,ContentDocument&,std::string& error) const;
    bool SavePersistent(SceneState&,Renderer&,const SceneAssetRoots&,std::string& error);
};
