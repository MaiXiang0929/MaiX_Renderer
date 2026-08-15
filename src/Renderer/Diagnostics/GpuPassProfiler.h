// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glad/glad.h>

#include "Renderer/Pipeline/RenderPass.h"

struct GpuPassTiming
{
    double lastMilliseconds = 0.0;
    double averageMilliseconds = 0.0;
    bool valid = false;
};

struct GpuTimingSnapshot
{
    static constexpr std::size_t PassCount = 8;

    std::array<GpuPassTiming, PassCount> passes{};
    double totalLastMilliseconds = 0.0;
    double totalAverageMilliseconds = 0.0;
    std::uint64_t resolvedFrameCount = 0;
    std::uint64_t skippedFrameCount = 0;
};

/// @brief 使用多帧 Timer Query 异步统计各 Render Pass 的 GPU 执行时间。
class GpuPassProfiler
{
public:
    GpuPassProfiler() = default;
    ~GpuPassProfiler();

    GpuPassProfiler(const GpuPassProfiler&) = delete;
    GpuPassProfiler& operator=(const GpuPassProfiler&) = delete;

    bool Init();
    void BeginFrame();
    void BeginPass(RenderPassType type);
    void EndPass();
    void EndFrame();

    const GpuTimingSnapshot& GetSnapshot() const { return m_Snapshot; }

private:
    static constexpr std::size_t BufferedFrameCount = 3;
    static constexpr std::uint64_t LogInterval = 120;
    static constexpr double SmoothingFactor = 0.1;

    struct QueryFrame
    {
        std::array<GLuint, GpuTimingSnapshot::PassCount> queries{};
        bool pending = false;
    };

    bool TryResolve(QueryFrame& frame);
    void LogSnapshot() const;

    std::array<QueryFrame, BufferedFrameCount> m_Frames{};
    GpuTimingSnapshot m_Snapshot;
    std::size_t m_WriteFrameIndex = 0;
    RenderPassType m_ActivePass = RenderPassType::Shadow;
    bool m_Initialized = false;
    bool m_FrameActive = false;
    bool m_PassActive = false;
};
