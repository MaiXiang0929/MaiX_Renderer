// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/SceneContent.h"
#include "Engine/Runtime/Engine/SceneContentPreparation.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <limits>

PreparedSceneContent::~PreparedSceneContent()
{
    if (!m_Renderer) return;
    for (const auto mesh:ownedMeshes) m_Renderer->DestroyMesh(mesh);
    for (int instances=1;instances>=0;--instances)
        for (const auto material:scene.sceneMaterials)
            if (m_Renderer->IsMaterialInstance(material)==(instances!=0)) m_Renderer->DestroyMaterial(material);
}
bool PrepareSceneContent(const SceneDocument& document,Renderer& renderer,const SceneAssetRoots& roots,
    const SceneContentOptions& options,std::unique_ptr<PreparedSceneContent>& output,std::string& error)
{
    if(output) { error="Prepared content output must be empty."; return false; }
    // Standalone scene compatibility path: same preparation/upload implementation, driven synchronously.
    std::unique_ptr<SceneCpuContent> cpu;
    ScenePreparationLimits limits;
    limits.cpuBytes=std::numeric_limits<std::size_t>::max();
    limits.resourceBytes=limits.cpuBytes; limits.sourceBytes=limits.cpuBytes;
    if(!PrepareSceneCpuContent(document,roots,limits,{},cpu,error)) return false;
    SceneContentUpload upload;
    if(!upload.Begin(std::move(cpu),renderer,options,error)) return false;
    while(!upload.Complete()) if(!upload.Step(1024,std::numeric_limits<std::size_t>::max(),1000,error)) return false;
    output=upload.Take(); return true;
}
