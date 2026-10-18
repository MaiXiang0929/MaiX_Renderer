// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/RHI/RHIDevice.h"

/// @brief 可选的 GPU 调试分组；驱动不支持 KHR_debug 时自动退化为空操作。
class GpuDebugScope
{
public:
    explicit GpuDebugScope(RHICommandList& commands, const char* name);
    ~GpuDebugScope();

    GpuDebugScope(const GpuDebugScope&) = delete;
    GpuDebugScope& operator=(const GpuDebugScope&) = delete;

private:
    RHICommandList& m_Commands;
    bool m_Active = false;
};
