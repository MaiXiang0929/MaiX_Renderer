// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/EngineSession.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Runtime/Engine/ScenePersistence.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Runtime/Engine/SceneFrameBuilder.h"
#include <iostream>
#include <algorithm>

EngineSession::EngineSession() = default;
EngineSession::~EngineSession()
{
    Shutdown();
}

bool EngineSession::Initialize()
{
    if (m_Renderer)
        return true;
    auto candidate = std::make_unique<Renderer>();
    if (!candidate->Init())
        return false;
    m_Renderer = std::move(candidate);
    return true;
}

void EngineSession::Render(RenderFrameData &frame)
{
    if (m_Renderer)
        m_Renderer->ExecutePipeline(frame);
}

RenderFrameData EngineSession::PrepareFrame(Camera &camera, unsigned width, unsigned height,
                                            bool sceneCameraView, const SceneFrameSettings &settings)
{
    if (!m_Renderer)
        return {};
    if (!IsValidEnvironmentSettings(m_Scene.environment) || !IsValidPostProcessSettings(m_Scene.postProcess) ||
        !IsValidShadowSettings(m_Scene.shadowSettings) ||
        !std::all_of(m_Scene.lights.begin(), m_Scene.lights.end(),
            [](const SceneLight& light) { return IsValidLightPhotometry(light.proxy); }) ||
        !RegisterSceneActors(m_Scene))
    {
        if (!m_RegistrationFailed)
            std::cerr << "[Scene] Invalid scene settings or actor registration failed; frame rejected.\n";
        m_RegistrationFailed = true;
        return {};
    }
    m_RegistrationFailed = false;
    SynchronizeSceneActors(m_Scene, *m_Renderer);
    return BuildSceneFrame(m_Scene, camera, width, height, sceneCameraView && m_Scene.camera.id.IsValid(),
                           settings);
}

void EngineSession::Shutdown()
{
    if (m_Renderer)
        DestroySceneResources(m_Scene, *m_Renderer);
    m_Scene = SceneState{};
    m_RegistrationFailed = false;
    m_Renderer.reset();
}
