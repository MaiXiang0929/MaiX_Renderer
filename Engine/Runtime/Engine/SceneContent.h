// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Runtime/Scene/SceneDocument.h"
#include <memory>
#include <unordered_map>
class Renderer;
struct SceneContentBindings
{
    std::unordered_map<std::uint64_t,ActorId> actors;
    std::unordered_map<std::uint64_t,MaterialHandle> materials;
};
struct SceneContentOptions
{
    std::string region;
    std::unordered_map<std::uint64_t,std::string> actorGuids;
    std::unordered_map<std::uint64_t,MaterialHandle> borrowedMaterials;
    std::uint64_t firstModelId=1;
};
// Renderer owns candidate GPU objects. This guard releases exactly the objects it created.
// Proxies are value-only and remain unpublished until a prepared render-scene update commits.
class PreparedSceneContent
{
public:
    explicit PreparedSceneContent(Renderer& renderer):m_Renderer(&renderer) {}
    ~PreparedSceneContent();
    PreparedSceneContent(const PreparedSceneContent&)=delete;
    PreparedSceneContent& operator=(const PreparedSceneContent&)=delete;
    void Release() noexcept { m_Renderer=nullptr; }
    SceneState scene;
    SceneContentBindings bindings;
    std::vector<PrimitiveSceneProxy> primitives;
    std::vector<LightSceneProxy> lights;
private:
    Renderer* m_Renderer;
};
bool PrepareSceneContent(const SceneDocument&, Renderer&, const SceneAssetRoots&,
    const SceneContentOptions&, std::unique_ptr<PreparedSceneContent>&, std::string& error);
struct SceneContentSelection
{
    std::vector<ActorId> actors;
    std::vector<MaterialHandle> materials;
    std::vector<MaterialHandle> sharedMaterials; // Capture references as placeholders, not copies of world values.
};
bool CaptureSceneContent(SceneState&, const Renderer&, const SceneAssetRoots&,
    const SceneContentSelection&, SceneDocument&, SceneContentBindings&, std::string& error);
