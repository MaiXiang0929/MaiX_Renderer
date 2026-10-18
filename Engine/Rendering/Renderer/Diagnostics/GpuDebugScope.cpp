// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Diagnostics/GpuDebugScope.h"
GpuDebugScope::GpuDebugScope(RHICommandList& commands,const char* name)
    : m_Commands(commands), m_Active(commands.PushDebugGroup(name)) {}
GpuDebugScope::~GpuDebugScope() { if(m_Active) m_Commands.PopDebugGroup(); }
