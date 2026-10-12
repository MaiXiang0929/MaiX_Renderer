// SPDX-License-Identifier: MIT
#include "Engine/Core/Platform/WindowContext.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

WindowContext::~WindowContext() { Shutdown(); }

bool WindowContext::Initialize(unsigned int width, unsigned int height,
    const char* title, std::string& error, bool visible)
{
    error.clear();
    if (m_Window) return true;
    if (width == 0 || height == 0) { error = "Invalid window extent"; return false; }
    if (!glfwInit()) { error = "Failed to initialize GLFW"; return false; }
    m_GlfwInitialized = true;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
    m_Window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr, nullptr);
    if (!m_Window) { error = "Failed to create GLFW window"; Shutdown(); return false; }
    glfwMakeContextCurrent(m_Window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        error = "Failed to initialize GLAD";
        Shutdown();
        return false;
    }
    return true;
}

void WindowContext::Shutdown()
{
    if (m_Window) { glfwDestroyWindow(m_Window); m_Window = nullptr; }
    if (m_GlfwInitialized) { glfwTerminate(); m_GlfwInitialized = false; }
}
