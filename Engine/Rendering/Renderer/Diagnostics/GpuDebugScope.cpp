// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Diagnostics/GpuDebugScope.h"

#include <cstring>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace
{
using PushDebugGroupProc = void (APIENTRY*)(
    GLenum source, GLuint id, GLsizei length, const GLchar* message);
using PopDebugGroupProc = void (APIENTRY*)();

constexpr GLenum DebugSourceApplication = 0x824A;

PushDebugGroupProc GetPushDebugGroup()
{
    static const PushDebugGroupProc function = []
    {
        auto result = reinterpret_cast<PushDebugGroupProc>(
            glfwGetProcAddress("glPushDebugGroup"));
        if (result == nullptr)
        {
            result = reinterpret_cast<PushDebugGroupProc>(
                glfwGetProcAddress("glPushDebugGroupKHR"));
        }
        return result;
    }();
    return function;
}

PopDebugGroupProc GetPopDebugGroup()
{
    static const PopDebugGroupProc function = []
    {
        auto result = reinterpret_cast<PopDebugGroupProc>(
            glfwGetProcAddress("glPopDebugGroup"));
        if (result == nullptr)
        {
            result = reinterpret_cast<PopDebugGroupProc>(
                glfwGetProcAddress("glPopDebugGroupKHR"));
        }
        return result;
    }();
    return function;
}
}

GpuDebugScope::GpuDebugScope(const char* name)
{
    const PushDebugGroupProc push = GetPushDebugGroup();
    const PopDebugGroupProc pop = GetPopDebugGroup();
    if (push == nullptr || pop == nullptr || name == nullptr)
        return;

    push(
        DebugSourceApplication,
        0,
        static_cast<GLsizei>(std::strlen(name)),
        name);
    m_Active = true;
}

GpuDebugScope::~GpuDebugScope()
{
    if (m_Active)
        GetPopDebugGroup()();
}
