// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Diagnostics/GpuPassProfiler.h"

#include <iomanip>
#include <iostream>

namespace
{
std::size_t ToIndex(RenderPassType type)
{
    return static_cast<std::size_t>(type);
}

const char* ToString(RenderPassType type)
{
    switch (type)
    {
    case RenderPassType::Shadow: return "Shadow";
    case RenderPassType::ShadowPreview: return "ShadowPreview";
    case RenderPassType::Reflection: return "Reflection";
    case RenderPassType::Forward: return "Forward";
    case RenderPassType::Outline: return "Outline";
    case RenderPassType::Translucency: return "Translucency";
    case RenderPassType::SSAO: return "SSAO";
    case RenderPassType::EditorPrimitive: return "EditorPrimitive";
    case RenderPassType::Bloom: return "Bloom";
    case RenderPassType::PostProcess: return "PostProcess";
    case RenderPassType::Present: return "Present";
    case RenderPassType::Count: break;
    }
    return "Unknown";
}
}

GpuPassProfiler::~GpuPassProfiler()
{
    if (!m_Initialized)
        return;

    for (QueryFrame& frame : m_Frames)
    {
        for (auto query : frame.queries) m_Device.DestroyQuery(query);
    }
}

bool GpuPassProfiler::Init()
{
    if (m_Initialized)
        return true;
    if (!m_Device.Capabilities().timerQueries)
    {
        std::cerr << "[GpuPassProfiler] Timer Query API is unavailable."
                  << std::endl;
        return false;
    }

    for (QueryFrame& frame : m_Frames)
    {
        for (auto& query : frame.queries) query=m_Device.CreateQuery();
        for (RHIQueryHandle query : frame.queries)
        {
            if (query == 0)
            {
                for (QueryFrame& cleanupFrame : m_Frames)
                {
                    for (RHIQueryHandle& cleanupQuery : cleanupFrame.queries)
                    {
                        if (cleanupQuery != 0)
                            m_Device.DestroyQuery(cleanupQuery);
                        cleanupQuery = 0;
                    }
                }
                return false;
            }
        }
    }

    m_Initialized = true;
    return true;
}

void GpuPassProfiler::BeginFrame()
{
    m_FrameActive = false;
    m_PassActive = false;
    if (!m_Initialized)
        return;

    QueryFrame& writeFrame = m_Frames[m_WriteFrameIndex];
    if (writeFrame.pending && !TryResolve(writeFrame))
    {
        // GPU 尚未完成该槽位时跳过整帧采样，避免覆盖 Query 或同步等待。
        ++m_Snapshot.skippedFrameCount;
        return;
    }

    writeFrame.issued.fill(false);
    m_FrameActive = true;
}

void GpuPassProfiler::BeginPass(RenderPassType type)
{
    if (!m_FrameActive)
        return;

    m_ActivePass = type;
    m_Device.Commands().BeginQuery(m_Frames[m_WriteFrameIndex].queries[ToIndex(type)]);
    m_Frames[m_WriteFrameIndex].issued[ToIndex(type)]=true;
    m_PassActive = true;
}

void GpuPassProfiler::EndPass()
{
    if (!m_PassActive)
        return;

    m_Device.Commands().EndQuery();
    m_PassActive = false;
}

void GpuPassProfiler::EndFrame()
{
    if (!m_FrameActive)
        return;

    m_Frames[m_WriteFrameIndex].pending = true;
    m_WriteFrameIndex = (m_WriteFrameIndex + 1) % BufferedFrameCount;
    m_FrameActive = false;
}

bool GpuPassProfiler::TryResolve(QueryFrame& frame)
{
    std::array<std::uint64_t,GpuTimingSnapshot::PassCount> results{};
    for (std::size_t i=0;i<frame.queries.size();++i)
        if(frame.issued[i] && !m_Device.TryQueryResult(frame.queries[i],results[i])) return false;

    m_Snapshot.totalLastMilliseconds = 0.0;
    m_Snapshot.totalAverageMilliseconds = 0.0;
    for (std::size_t index = 0; index < frame.queries.size(); ++index)
    {
        if(!frame.issued[index]) { m_Snapshot.passes[index].valid=false; continue; }
        const std::uint64_t elapsedNanoseconds=results[index];
        const double milliseconds =
            static_cast<double>(elapsedNanoseconds) / 1.0e6;

        GpuPassTiming& timing = m_Snapshot.passes[index];
        timing.lastMilliseconds = milliseconds;
        timing.averageMilliseconds = timing.valid
            ? timing.averageMilliseconds +
                SmoothingFactor * (milliseconds - timing.averageMilliseconds)
            : milliseconds;
        timing.valid = true;
        m_Snapshot.totalLastMilliseconds += timing.lastMilliseconds;
        m_Snapshot.totalAverageMilliseconds += timing.averageMilliseconds;
    }

    frame.pending = false;
    ++m_Snapshot.resolvedFrameCount;
    if (m_Snapshot.resolvedFrameCount == 1 ||
        m_Snapshot.resolvedFrameCount % LogInterval == 0)
    {
        LogSnapshot();
    }
    return true;
}

void GpuPassProfiler::LogSnapshot() const
{
    const std::ios::fmtflags previousFlags = std::cout.flags();
    const std::streamsize previousPrecision = std::cout.precision();
    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t index = 0; index < m_Snapshot.passes.size(); ++index)
    {
        const GpuPassTiming& timing = m_Snapshot.passes[index];
        std::cout
            << "[GpuTiming]["
            << ToString(static_cast<RenderPassType>(index)) << "]"
            << " lastMs=" << timing.lastMilliseconds
            << " avgMs=" << timing.averageMilliseconds
            << std::endl;
    }
    std::cout
        << "[GpuTiming][Frame] lastMs="
        << m_Snapshot.totalLastMilliseconds
        << " avgMs=" << m_Snapshot.totalAverageMilliseconds
        << " resolved=" << m_Snapshot.resolvedFrameCount
        << " skipped=" << m_Snapshot.skippedFrameCount
        << std::endl;
    std::cout.flags(previousFlags);
    std::cout.precision(previousPrecision);
}
