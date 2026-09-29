// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Renderer/Resources/Framebuffer.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[FramebufferTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

GLuint BoundFramebuffer()
{
    GLint value = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    return static_cast<GLuint>(value);
}

GLuint BoundTexture()
{
    GLint value = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    return static_cast<GLuint>(value);
}
}

int main()
{
    if (!glfwInit())
    {
        std::cout << "FramebufferTests skipped: GLFW context unavailable."
                  << std::endl;
        return EXIT_SUCCESS;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "FramebufferTests", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        std::cout << "FramebufferTests skipped: OpenGL 4.0 context unavailable."
                  << std::endl;
        return EXIT_SUCCESS;
    }
    glfwMakeContextCurrent(window);
    Require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0,
        "GLAD initialization failed.");

    {
        Framebuffer target;
        FramebufferSpecification specification;
        specification.width = 64;
        specification.height = 48;
        specification.colorFormat = FramebufferColorFormat::RGBA16F;
        specification.sampleableDepth = true;
        specification.mipmapsEnabled = false;
        Require(target.Init(specification), "Initial framebuffer creation failed.");

        const GLuint oldColor = target.GetColorTexture();
        const GLuint oldDepth = target.GetDepthTexture();
        target.Bind();
        const GLuint oldFramebuffer = BoundFramebuffer();
        glBindTexture(GL_TEXTURE_2D, oldColor);
        Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "Initial framebuffer is incomplete.");

        GLint maxTextureSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        Require(maxTextureSize > 0, "Invalid maximum texture size.");
        FramebufferSpecification oversized = specification;
        oversized.width = maxTextureSize + 1;
        oversized.colorFormat = FramebufferColorFormat::RGBA8;
        oversized.depthStencilEnabled = false;
        oversized.sampleableDepth = false;
        Require(!target.Init(oversized),
            "Oversized framebuffer creation should fail.");
        Require(target.GetWidth() == 64 && target.GetHeight() == 48 &&
                target.GetColorTexture() == oldColor &&
                target.GetDepthTexture() == oldDepth &&
                target.GetColorFormat() == FramebufferColorFormat::RGBA16F &&
                target.HasDepthTexture(),
            "Failed resize must preserve the previous attachments and specification.");
        Require(BoundFramebuffer() == oldFramebuffer && BoundTexture() == oldColor,
            "Failed resize must restore the previous OpenGL bindings.");
        Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "Previous framebuffer must remain complete after a failed resize.");
        while (glGetError() != GL_NO_ERROR) {}

        specification.width = 96;
        specification.height = 80;
        Require(target.Init(specification), "Valid framebuffer resize failed.");
        Require(target.GetWidth() == 96 && target.GetHeight() == 80 &&
                target.GetColorTexture() != oldColor &&
                target.GetDepthTexture() != oldDepth,
            "Successful resize must commit new attachments and extent.");
        Require(BoundFramebuffer() != oldFramebuffer &&
                BoundTexture() == target.GetColorTexture(),
            "Successful resize must remap bindings to the new resources.");
        Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "Replacement framebuffer is incomplete.");
        Require(glIsTexture(oldColor) == GL_FALSE &&
                glIsTexture(oldDepth) == GL_FALSE,
            "Successful resize must release the previous attachments.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "FramebufferTests passed." << std::endl;
    return EXIT_SUCCESS;
}
