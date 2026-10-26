// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Runtime/Engine/WorldContentSession.h"
#include "Engine/Runtime/Engine/WorldStreaming.h"
#include <memory>

class Renderer;
struct RenderFrameData;
struct SceneFrameSettings;
class Camera;

// The host owns the context; this session owns scene values and the renderer.
// Initialize / Render / Shutdown must run with that context current.
class EngineSession
{
public:
    EngineSession();
    ~EngineSession();
    EngineSession(const EngineSession&) = delete;
    EngineSession& operator=(const EngineSession&) = delete;
    bool Initialize();
    RenderFrameData PrepareFrame(Camera& camera, unsigned width, unsigned height,
        bool sceneCameraView, const SceneFrameSettings& settings);
    void Render(RenderFrameData& frame);
    void Shutdown();
    Renderer* GetRenderer() const { return m_Renderer.get(); }
    SceneState& Scene() { return m_Scene; }
    const SceneState& Scene() const { return m_Scene; }
    WorldContentSession& Content() { return m_Content; }
    const WorldContentSession& Content() const { return m_Content; }
    WorldStreaming& Streaming() { return m_Streaming; }
private:
    SceneState m_Scene;
    WorldContentSession m_Content;
    std::unique_ptr<Renderer> m_Renderer;
    WorldStreaming m_Streaming;
    bool m_RegistrationFailed = false;
};
