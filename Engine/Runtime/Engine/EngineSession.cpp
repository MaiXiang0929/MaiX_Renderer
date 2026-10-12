// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/EngineSession.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"

EngineSession::EngineSession() = default;
EngineSession::~EngineSession() { Shutdown(); }

bool EngineSession::Initialize()
{
    if (m_Renderer) return true;
    auto candidate = std::make_unique<Renderer>();
    if (!candidate->Init()) return false;
    m_Renderer = std::move(candidate);
    return true;
}

void EngineSession::Render(RenderFrameData& frame)
{
    if (m_Renderer) m_Renderer->ExecutePipeline(frame);
}

void EngineSession::Shutdown()
{
    if (m_Renderer)
        for (auto& model : m_Scene.models)
            DestroySceneModelResources(model, *m_Renderer);
    m_Scene = SceneState{};
    m_Renderer.reset();
}
