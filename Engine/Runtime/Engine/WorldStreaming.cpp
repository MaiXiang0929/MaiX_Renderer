// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/WorldStreaming.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <map>

namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
double Distance(const ContentRegionRecord& region,const cy::Vec3f& source)
{
    double squared=0;
    for(int i=0;i<3;++i) { const auto d=std::max({double(region.minimum[i])-source[i],0.0,double(source[i])-region.maximum[i]}); squared+=d*d; }
    return std::sqrt(squared);
}
bool Busy(WorldStreamPhase phase)
{ return phase==WorldStreamPhase::Queued||phase==WorldStreamPhase::Preparing||phase==WorldStreamPhase::WaitingBudget||phase==WorldStreamPhase::Uploading||phase==WorldStreamPhase::Activating||phase==WorldStreamPhase::Cancelling; }
}
const char* WorldStreamPhaseName(WorldStreamPhase phase)
{
    switch(phase) {
    case WorldStreamPhase::Unloaded:return "Unloaded"; case WorldStreamPhase::Queued:return "Queued";
    case WorldStreamPhase::Preparing:return "Preparing CPU content"; case WorldStreamPhase::WaitingBudget:return "Waiting for budget";
    case WorldStreamPhase::Uploading:return "Uploading"; case WorldStreamPhase::Activating:return "Awaiting activation";
    case WorldStreamPhase::Loaded:return "Loaded"; case WorldStreamPhase::Cancelling:return "Cancelling";
    case WorldStreamPhase::Cancelled:return "Cancelled"; case WorldStreamPhase::Failed:return "Failed";
    } return "Unknown";
}
struct WorldStreaming::Impl
{
    struct WorkerResult { RegionLoadDocument loaded; std::unique_ptr<SceneCpuContent> cpu; std::string error; };
    struct Request {
        WorldStreamStatus status;
        std::shared_ptr<std::atomic_bool> cancel;
        std::future<WorkerResult> worker;
        RegionLoadDocument loaded;
        std::unique_ptr<SceneCpuContent> cpu;
        std::unique_ptr<SceneContentUpload> upload;
        std::size_t cpuBytes=0,gpuBytes=0,reservedCpuBytes=0;
        bool suppress=false,unload=false,automaticUnload=false,pressureUnload=false;
        Clock::time_point started=Clock::now(),retryAfter{};
        void Queue() {
            const bool held=status.manuallyHeld; status={}; status.manuallyHeld=held;
            status.phase=WorldStreamPhase::Queued; started=Clock::now(); cancel.reset();
        }
    };
    WorldStreamingSettings settings;
    WorldStreamingStatistics stats;
    std::uint64_t generation=0;
    std::map<std::string,Request> requests;
    struct Obsolete { std::future<WorkerResult> worker; std::size_t reserved; };
    std::vector<Obsolete> obsolete;
    std::vector<SceneContentRetirement> retired;
    void Synchronize(WorldContentSession&,Renderer&);
    void Stop(Renderer&);
    bool ReleaseOne(Renderer&);
    void Abandon(Request& request) {
        if(request.cancel) request.cancel->store(true);
        if(request.upload) { retired.push_back(request.upload->Abandon()); request.upload.reset(); }
        request.cpu.reset(); request.loaded={}; request.cpuBytes=0; request.gpuBytes=0;
        request.status.phase=request.worker.valid()?WorldStreamPhase::Cancelling:WorldStreamPhase::Cancelled;
    }
    std::size_t CpuReserved() const {
        std::size_t bytes=0; for(const auto& old:obsolete) bytes+=old.reserved;
        for(const auto& item:requests) bytes+=item.second.worker.valid()?item.second.reservedCpuBytes:item.second.cpuBytes;
        return bytes;
    }
    std::size_t QueueSize() const {
        return std::count_if(requests.begin(),requests.end(),[](const auto& item) { return Busy(item.second.status.phase); });
    }
};
WorldStreaming::WorldStreaming():m_Impl(std::make_unique<Impl>()) {}
WorldStreaming::~WorldStreaming()=default;
const WorldStreamingSettings& WorldStreaming::Settings() const { return m_Impl->settings; }
const WorldStreamingStatistics& WorldStreaming::Statistics() const { return m_Impl->stats; }
const WorldStreamStatus* WorldStreaming::Status(const std::string& id) const
{ const auto it=m_Impl->requests.find(id); return it==m_Impl->requests.end()?nullptr:&it->second.status; }
bool WorldStreaming::Configure(const WorldStreamingSettings& s,std::string& error)
{
    if(!std::isfinite(s.loadDistance)||!std::isfinite(s.unloadDistance)||s.loadDistance<0||s.unloadDistance<=s.loadDistance||
        !std::isfinite(s.uploadMilliseconds)||s.uploadMilliseconds<=0||s.uploadMilliseconds>1000||!s.operationsPerFrame||
        s.operationsPerFrame>100000||!s.uploadBytesPerFrame||!s.maxRequests||s.maxRequests>1024||!s.cpuCandidateBytes||
        !s.gpuSceneBytes||!s.preparation.cpuBytes||!s.preparation.resourceBytes||!s.preparation.sourceBytes) {
        error="Invalid streaming distances or budgets."; return false;
    }
    m_Impl->settings=s; error.clear(); return true;
}
bool WorldStreaming::Impl::ReleaseOne(Renderer& renderer)
{
    while(!retired.empty()) {
        auto& resources=retired.front();
        if(!resources.meshes.empty()) {
            auto handle=resources.meshes.back();
            if(renderer.HasMesh(handle)&&!renderer.DestroyMesh(handle)) return false;
            resources.meshes.pop_back(); return true;
        }
        if(!resources.materials.empty()) {
            // Stored instance-first; consume in that same dependency order.
            auto handle=resources.materials.front(); Renderer::MaterialSnapshot snapshot;
            if(renderer.GetMaterialSnapshot(handle,snapshot)&&!renderer.DestroyMaterial(handle)) return false;
            resources.materials.erase(resources.materials.begin()); return true;
        }
        if(!resources.textures.empty()) { resources.textures.pop_back(); return true; }
        retired.erase(retired.begin());
    }
    return false;
}
void WorldStreaming::Impl::Stop(Renderer& renderer)
{
    for(auto& entry:requests) {
        Abandon(entry.second);
        if(entry.second.worker.valid()) obsolete.push_back({std::move(entry.second.worker),entry.second.reservedCpuBytes});
    }
    requests.clear(); generation=0;
    while(ReleaseOne(renderer)) {}
}
void WorldStreaming::Impl::Synchronize(WorldContentSession& content,Renderer& renderer)
{
    if(generation==content.Generation()) return;
    Stop(renderer); generation=content.Generation(); settings.automatic=false;
    // Content already loaded by a caller is explicitly held; enabling automatic streaming never silently evicts it.
    for(const auto& region:content.Regions()) if(region.loaded) {
        auto& request=requests[region.record.id]; request.status.phase=WorldStreamPhase::Loaded; request.status.manuallyHeld=true;
    }
}
bool WorldStreaming::RequestLoad(const std::string& id,WorldContentSession& content,Renderer& renderer,std::string& error)
{
    auto& p=*m_Impl; p.Synchronize(content,renderer);
    auto region=std::find_if(content.Regions().begin(),content.Regions().end(),[&](const auto& r) { return r.record.id==id; });
    if(!content.Ready()||region==content.Regions().end()) { error="No matching region in the active world."; return false; }
    auto& request=p.requests[id];
    if(!region->loaded&&!Busy(request.status.phase)&&p.QueueSize()>=p.settings.maxRequests) { error="Streaming request queue is full."; return false; }
    request.status.manuallyHeld=true; request.unload=false; request.suppress=false; request.status.message.clear();
    if(region->loaded) request.status.phase=WorldStreamPhase::Loaded;
    else if(!Busy(request.status.phase)) request.Queue();
    else if(request.cancel&&request.cancel->load()) { error="Previous request is cancelling; retry after it completes."; return false; }
    error.clear(); return true;
}
bool WorldStreaming::RequestUnload(const std::string& id,WorldContentSession& content,Renderer& renderer,std::string& error)
{
    auto& p=*m_Impl; p.Synchronize(content,renderer);
    auto region=std::find_if(content.Regions().begin(),content.Regions().end(),[&](const auto& r) { return r.record.id==id; });
    if(region==content.Regions().end()) { error="Unknown region."; return false; }
    auto& request=p.requests[id]; request.status.manuallyHeld=false; request.suppress=true; request.unload=region->loaded; request.automaticUnload=false;
    if(!region->loaded) p.Abandon(request);
    error.clear(); return true;
}
void WorldStreaming::Cancel(const std::string& id)
{
    auto found=m_Impl->requests.find(id); if(found==m_Impl->requests.end()) return;
    auto& request=found->second; request.suppress=true; request.status.manuallyHeld=false;
    if(request.cancel) request.cancel->store(true);
    if(Busy(request.status.phase)) request.status.phase=WorldStreamPhase::Cancelling;
}
void WorldStreaming::Stop(Renderer& renderer) { m_Impl->Stop(renderer); }
void WorldStreaming::ReleaseHold(const std::string& id)
{
    auto found=m_Impl->requests.find(id);
    if(found!=m_Impl->requests.end()) { found->second.status.manuallyHeld=false; found->second.suppress=false; }
}
void WorldStreaming::Shutdown(Renderer& renderer)
{
    auto& p=*m_Impl; p.Stop(renderer);
    for(auto& worker:p.obsolete) if(worker.worker.valid()) worker.worker.wait();
    p.obsolete.clear(); while(p.ReleaseOne(renderer)) {}
}
void WorldStreaming::Update(WorldContentSession& content,SceneState& scene,Renderer& renderer,const SceneAssetRoots& roots,const cy::Vec3f& source)
{
    auto& p=*m_Impl; const auto tick=Clock::now(); p.Synchronize(content,renderer);
    for(auto it=p.obsolete.begin();it!=p.obsolete.end();) {
        if(it->worker.wait_for(std::chrono::seconds(0))==std::future_status::ready) { it->worker.get(); it=p.obsolete.erase(it); } else ++it;
    }
    // Release budget is shared with uploading below; neither path blocks on the GPU.
    const auto releaseStart=Clock::now(); std::size_t released=0;
    while(released<p.settings.operationsPerFrame&&Ms(releaseStart)<p.settings.uploadMilliseconds) {
        if(!p.ReleaseOne(renderer)) break; ++released;
    }
    p.stats.retirementMilliseconds=Ms(releaseStart);
    if(!content.Ready()||scene.contentWorld!=content.Id()) return;
    const bool validSource=std::isfinite(source.x)&&std::isfinite(source.y)&&std::isfinite(source.z);
    std::vector<const ContentRegionStatus*> ordered;
    for(const auto& region:content.Regions()) ordered.push_back(&region);
    std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b) { return validSource&&Distance(a->record,source)<Distance(b->record,source); });
    bool transitioned=false;
    for(const auto* region:ordered) {
        const auto& id=region->record.id;
        auto [entry,inserted]=p.requests.try_emplace(id); auto& request=entry->second;
        // Regions created after session binding are already loaded by an explicit authoring action.
        if(inserted&&region->loaded) { request.status.phase=WorldStreamPhase::Loaded; request.status.manuallyHeld=true; }
        const double distance=validSource?Distance(region->record,source):0;
        if(validSource&&distance>p.settings.unloadDistance) request.suppress=false;
        if(request.status.phase==WorldStreamPhase::Cancelling&&!request.worker.valid()) p.Abandon(request);
        if(request.worker.valid()&&request.worker.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            auto result=request.worker.get();
            if(request.cancel->load()) { request.status.phase=WorldStreamPhase::Cancelled; request.cpuBytes=0; }
            else if(!result.cpu) { request.status.phase=WorldStreamPhase::Failed; request.status.message=result.error; request.suppress=true; }
            else {
                request.status.cpuMilliseconds=result.cpu->preparationMs;
                request.cpuBytes=result.cpu->cpuBytes; request.gpuBytes=result.cpu->gpuBytes;
                request.loaded=std::move(result.loaded); request.cpu=std::move(result.cpu); request.status.phase=WorldStreamPhase::WaitingBudget;
            }
        }
        if(p.settings.automatic&&validSource&&!request.status.manuallyHeld) {
            if(!region->loaded&&distance<=p.settings.loadDistance&&!request.suppress&&!Busy(request.status.phase)&&
                request.status.phase!=WorldStreamPhase::Failed&&p.QueueSize()<p.settings.maxRequests) {
                request.Queue();
            }
            if(distance>p.settings.unloadDistance) {
                if(!region->loaded&&Busy(request.status.phase)) { request.suppress=false; p.Abandon(request); }
                if(region->loaded) { request.unload=true; request.automaticUnload=true; }
                if(request.status.phase==WorldStreamPhase::Failed) request.status.phase=WorldStreamPhase::Unloaded;
            } else if(region->loaded&&request.automaticUnload&&(!request.pressureUnload||distance<=p.settings.loadDistance)) request.unload=false;
        } else if(!p.settings.automatic&&!request.status.manuallyHeld&&Busy(request.status.phase)) p.Abandon(request);
        if(!p.settings.automatic&&request.automaticUnload) request.unload=false;
        if(request.unload&&region->loaded&&Clock::now()>=request.retryAfter) {
            if(request.automaticUnload&&scene.world.ContentRegion()==id) { request.status.message="Current editing region is held."; continue; }
            if(scene.world.ContentRegionPins(id)) { request.status.message="Undo/Redo retains this region."; request.retryAfter=Clock::now()+std::chrono::milliseconds(250); continue; }
            SceneContentRetirement resources; std::string error;
            if(content.DetachRegion(id,scene,renderer,roots,resources,error)) {
                p.retired.push_back(std::move(resources)); request.status.phase=WorldStreamPhase::Unloaded; request.status.message.clear(); request.unload=false;
            } else { request.status.message=error; request.retryAfter=Clock::now()+std::chrono::milliseconds(250); if(!request.automaticUnload) request.unload=false; }
            transitioned=true;
            // At most one whole-region detach or activation per tick.
            break;
        }
    }
    bool workerBusy=!p.obsolete.empty(),uploadBusy=false;
    for(const auto& item:p.requests) { workerBusy=workerBusy||item.second.worker.valid(); uploadBusy=uploadBusy||static_cast<bool>(item.second.upload); }
    for(const auto* region:ordered) {
        auto& request=p.requests[region->record.id]; std::string error;
        switch(request.status.phase) {
        case WorldStreamPhase::Queued:
            if(!workerBusy) {
                auto limits=p.settings.preparation; limits.cpuBytes=std::min(limits.cpuBytes,p.settings.cpuCandidateBytes);
                const auto reserved=p.CpuReserved();
                if(reserved>p.settings.cpuCandidateBytes||limits.cpuBytes>p.settings.cpuCandidateBytes-reserved) { request.status.message="Waiting for CPU candidate budget."; break; }
                RegionLoadDescription description;
                if(!content.DescribeRegionLoad(region->record.id,roots,description,error)) { request.status.phase=WorldStreamPhase::Failed; request.status.message=error; break; }
                request.cancel=std::make_shared<std::atomic_bool>(false); auto cancel=request.cancel;
                request.reservedCpuBytes=limits.cpuBytes;
                request.worker=std::async(std::launch::async,[description=std::move(description),roots,limits,cancel] {
                    Impl::WorkerResult result;
                    const auto cpuStart=Clock::now();
                    try {
                        if(ReadRegionLoadDocument(description,result.loaded,result.error)&&!cancel->load())
                            PrepareSceneCpuContent(result.loaded.region.content.scene,roots,limits,cancel,result.cpu,result.error);
                    } catch(const std::exception& e) { result.error=e.what(); }
                    if(result.cpu) result.cpu->preparationMs=Ms(cpuStart);
                    return result;
                });
                request.status.phase=WorldStreamPhase::Preparing; request.status.message.clear(); workerBusy=true;
            }
            break;
        case WorldStreamPhase::WaitingBudget:
            if(!uploadBusy) {
                if(request.gpuBytes>p.settings.gpuSceneBytes) { request.status.message="Region exceeds total scene GPU budget; increase budget or reduce content."; break; }
                const auto resident=renderer.EstimatedSceneResourceBytes();
                std::size_t orphanTextures=0; for(const auto& retired:p.retired) for(const auto& t:retired.textures) if(t) orphanTextures+=t->EstimatedBytes();
                if(resident>p.settings.gpuSceneBytes||orphanTextures>p.settings.gpuSceneBytes-resident||request.gpuBytes>p.settings.gpuSceneBytes-resident-orphanTextures) {
                    request.status.message="GPU budget full; held/nearby regions cannot be evicted.";
                    if(p.settings.automatic&&validSource) for(auto it=ordered.rbegin();it!=ordered.rend();++it) {
                        auto& other=p.requests[(*it)->record.id];
                        if((*it)->loaded&&!other.status.manuallyHeld&&Distance((*it)->record,source)>p.settings.loadDistance) { other.unload=true; other.automaticUnload=true; other.pressureUnload=true; break; }
                    }
                    break;
                }
                SceneContentOptions options;
                if(!content.RegionOptions(request.loaded,scene,options,error)) { request.status.phase=WorldStreamPhase::Failed; request.status.message=error; request.cpu.reset(); request.cpuBytes=0; break; }
                const auto count=request.cpu->models.size();
                if(count>std::numeric_limits<std::uint64_t>::max()-scene.nextModelId) { request.status.phase=WorldStreamPhase::Failed; request.status.message="Model identity space exhausted."; request.cpu.reset(); request.cpuBytes=0; break; }
                // Reserve a runtime-only identity range; edits during upload use subsequent IDs.
                scene.nextModelId+=count; request.upload=std::make_unique<SceneContentUpload>();
                if(!request.upload->Begin(std::move(request.cpu),renderer,options,error)) { p.Abandon(request); request.status.phase=WorldStreamPhase::Failed; request.status.message=error; break; }
                request.status.total=request.upload->TotalOperations(); request.status.completed=0;
                request.status.phase=WorldStreamPhase::Uploading; request.status.message.clear(); uploadBusy=true;
            }
            break;
        case WorldStreamPhase::Uploading:
            if(released<p.settings.operationsPerFrame&&Ms(releaseStart)<p.settings.uploadMilliseconds) {
                if(!request.upload->Step(p.settings.operationsPerFrame-released,p.settings.uploadBytesPerFrame,
                    p.settings.uploadMilliseconds-Ms(releaseStart),error)) {
                    p.Abandon(request); request.status.phase=WorldStreamPhase::Failed; request.status.message=error;
                } else {
                    request.status.uploadMilliseconds+=request.upload->LastStepMs(); request.status.completed=request.upload->CompletedOperations();
                    if(request.upload->Complete()) request.status.phase=WorldStreamPhase::Activating;
                }
            }
            break;
        case WorldStreamPhase::Activating:
            if(!transitioned) {
                const auto start=Clock::now(); auto prepared=request.upload->Take();
                if(!content.CommitRegion(request.loaded,*prepared,scene,renderer,error)) {
                    SceneContentRetirement cleanup; cleanup.meshes=std::move(prepared->ownedMeshes); cleanup.materials=std::move(prepared->scene.sceneMaterials);
                    std::stable_sort(cleanup.materials.begin(),cleanup.materials.end(),[&](auto a,auto b) { return renderer.IsMaterialInstance(a)>renderer.IsMaterialInstance(b); });
                    prepared->Release(); p.retired.push_back(std::move(cleanup));
                    auto textures=request.upload->Abandon(); p.retired.push_back(std::move(textures));
                    request.status.phase=WorldStreamPhase::Failed; request.status.message=error;
                } else request.status.phase=WorldStreamPhase::Loaded;
                request.status.activationMilliseconds=Ms(start); request.status.latencyMilliseconds=Ms(request.started);
                request.upload.reset(); request.loaded={}; request.cpuBytes=0; request.gpuBytes=0; transitioned=true;
            }
            break;
        default:break;
        }
    }
    p.stats.queueDepth=p.QueueSize(); p.stats.cpuBytes=p.CpuReserved(); p.stats.gpuUpperEstimate=renderer.EstimatedSceneResourceBytes();
    for(const auto& entry:p.requests) if(entry.second.upload) p.stats.gpuUpperEstimate+=entry.second.gpuBytes;
    for(const auto& retired:p.retired) for(const auto& t:retired.textures) if(t) p.stats.gpuUpperEstimate+=t->EstimatedBytes();
    p.stats.peakQueueDepth=std::max(p.stats.peakQueueDepth,p.stats.queueDepth);
    p.stats.peakCpuBytes=std::max(p.stats.peakCpuBytes,p.stats.cpuBytes);
    p.stats.peakGpuUpperEstimate=std::max(p.stats.peakGpuUpperEstimate,p.stats.gpuUpperEstimate);
    p.stats.tickMilliseconds=Ms(tick); p.stats.peakTickMilliseconds=std::max(p.stats.peakTickMilliseconds,p.stats.tickMilliseconds);
}
