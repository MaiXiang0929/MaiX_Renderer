// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/SceneState.h"
#include <memory>

class Renderer;
struct RenderFrameData;

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
    void Render(RenderFrameData& frame);
    void Shutdown();
    Renderer* GetRenderer() const { return m_Renderer.get(); }
    SceneState& Scene() { return m_Scene; }
    const SceneState& Scene() const { return m_Scene; }
private:
    SceneState m_Scene;
    std::unique_ptr<Renderer> m_Renderer;
};
