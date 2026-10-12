// SPDX-License-Identifier: MIT
#pragma once

/// @brief 可选的 GPU 调试分组；驱动不支持 KHR_debug 时自动退化为空操作。
class GpuDebugScope
{
public:
    explicit GpuDebugScope(const char* name);
    ~GpuDebugScope();

    GpuDebugScope(const GpuDebugScope&) = delete;
    GpuDebugScope& operator=(const GpuDebugScope&) = delete;

private:
    bool m_Active = false;
};
