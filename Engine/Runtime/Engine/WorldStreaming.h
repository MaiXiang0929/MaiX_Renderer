// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Engine/SceneContentPreparation.h"
#include "Engine/Runtime/Engine/WorldContentSession.h"

enum class WorldStreamPhase { Unloaded,Queued,Preparing,WaitingBudget,Uploading,Activating,Loaded,Cancelling,Cancelled,Failed };
const char* WorldStreamPhaseName(WorldStreamPhase);
struct WorldStreamingSettings
{
    bool automatic=false;
    float loadDistance=100.0f,unloadDistance=150.0f;
    double uploadMilliseconds=2.0;
    std::size_t operationsPerFrame=8,uploadBytesPerFrame=4u*1024u*1024u;
    std::size_t maxRequests=32,cpuCandidateBytes=512u*1024u*1024u,gpuSceneBytes=512u*1024u*1024u;
    ScenePreparationLimits preparation;
};
struct WorldStreamStatus
{
    WorldStreamPhase phase=WorldStreamPhase::Unloaded;
    bool manuallyHeld=false;
    std::string message;
    std::size_t completed=0,total=0;
    double cpuMilliseconds=0,uploadMilliseconds=0,activationMilliseconds=0,latencyMilliseconds=0;
};
struct WorldStreamingStatistics
{
    std::size_t queueDepth=0,cpuBytes=0,gpuUpperEstimate=0;
    std::size_t peakQueueDepth=0,peakCpuBytes=0,peakGpuUpperEstimate=0;
    double tickMilliseconds=0,peakTickMilliseconds=0,retirementMilliseconds=0;
};
// EngineSession-owned scheduling only. Worker jobs own CPU values; Renderer remains GPU owner.
// One CPU preparation and one GPU candidate at a time bound concurrent parser memory/work.
class WorldStreaming
{
public:
    WorldStreaming();
    ~WorldStreaming();
    const WorldStreamingSettings& Settings() const;
    bool Configure(const WorldStreamingSettings&,std::string& error);
    const WorldStreamingStatistics& Statistics() const;
    const WorldStreamStatus* Status(const std::string& region) const;
    bool RequestLoad(const std::string&,WorldContentSession&,Renderer&,std::string& error);
    bool RequestUnload(const std::string&,WorldContentSession&,Renderer&,std::string& error);
    void Cancel(const std::string&);
    void ReleaseHold(const std::string&);
    void Update(WorldContentSession&,SceneState&,Renderer&,const SceneAssetRoots&,const cy::Vec3f& source);
    // Stop on context thread before whole-scene replacement; CPU work can finish without touching the old session.
    void Stop(Renderer&);
    // Join CPU jobs and drain candidate GPU resources before Renderer/context destruction.
    void Shutdown(Renderer&);
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
