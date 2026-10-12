// SPDX-License-Identifier: MIT
#pragma once
#include <string>
struct GLFWwindow;

// Single-window GLFW/OpenGL host. Destroy Engine/UI GPU resources before Shutdown.
class WindowContext
{
public:
    WindowContext() = default;
    ~WindowContext();
    WindowContext(const WindowContext&) = delete;
    WindowContext& operator=(const WindowContext&) = delete;
    bool Initialize(unsigned int width, unsigned int height, const char* title,
        std::string& error, bool visible = true);
    void Shutdown();
    GLFWwindow* GetWindow() const { return m_Window; }
private:
    GLFWwindow* m_Window = nullptr;
    bool m_GlfwInitialized = false;
};
