// SPDX-License-Identifier: MIT
#include "GpuPassProfiler.h"

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
    case RenderPassType::Reflection: return "Reflection";
    case RenderPassType::Forward: return "Forward";
    case RenderPassType::Outline: return "Outline";
    case RenderPassType::Translucency: return "Translucency";
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
        glDeleteQueries(
            static_cast<GLsizei>(frame.queries.size()),
            frame.queries.data());
    }
}

bool GpuPassProfiler::Init()
{
    if (m_Initialized)
        return true;
    if (glGenQueries == nullptr || glBeginQuery == nullptr ||
        glEndQuery == nullptr || glGetQueryObjectuiv == nullptr ||
        glGetQueryObjectui64v == nullptr)
    {
        std::cerr << "[GpuPassProfiler] Timer Query API is unavailable."
                  << std::endl;
        return false;
    }

    for (QueryFrame& frame : m_Frames)
    {
        glGenQueries(
            static_cast<GLsizei>(frame.queries.size()),
            frame.queries.data());
        for (GLuint query : frame.queries)
        {
            if (query == 0)
            {
                for (QueryFrame& cleanupFrame : m_Frames)
                {
                    for (GLuint& cleanupQuery : cleanupFrame.queries)
                    {
                        if (cleanupQuery != 0)
                            glDeleteQueries(1, &cleanupQuery);
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

    m_FrameActive = true;
}

void GpuPassProfiler::BeginPass(RenderPassType type)
{
    if (!m_FrameActive)
        return;

    m_ActivePass = type;
    glBeginQuery(
        GL_TIME_ELAPSED,
        m_Frames[m_WriteFrameIndex].queries[ToIndex(type)]);
    m_PassActive = true;
}

void GpuPassProfiler::EndPass()
{
    if (!m_PassActive)
        return;

    glEndQuery(GL_TIME_ELAPSED);
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
    for (GLuint query : frame.queries)
    {
        GLuint available = GL_FALSE;
        glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available != GL_TRUE)
            return false;
    }

    m_Snapshot.totalLastMilliseconds = 0.0;
    m_Snapshot.totalAverageMilliseconds = 0.0;
    for (std::size_t index = 0; index < frame.queries.size(); ++index)
    {
        GLuint64 elapsedNanoseconds = 0;
        glGetQueryObjectui64v(
            frame.queries[index], GL_QUERY_RESULT, &elapsedNanoseconds);
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
