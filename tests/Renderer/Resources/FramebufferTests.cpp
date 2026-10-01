// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <cmath>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Renderer/Resources/Framebuffer.h"
#include "Renderer/Pipeline/PassResourceContract.h"

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

void TestPassAttachmentActions()
{
    Framebuffer target;
    FramebufferSpecification specification;
    specification.width = 64;
    specification.height = 48;
    specification.colorFormat = FramebufferColorFormat::RGBA16F;
    specification.sampleableDepth = true;
    specification.mipmapsEnabled = false;
    Require(target.Init(specification), "Pass attachment target creation failed.");
    using Id = PassResourceId;
    FrameResources frame;
    frame.BeginFrame(64, 48);
    frame.Bind(Id::Environment, {0, 0, 0, 0, 0, ResourceFormat::Cubemap, false, true});
    const RenderResourceBinding color{target.GetColorTexture(), target.GetFramebufferId(),
        target.GetDepthTexture(), 64, 48, ResourceFormat::RGBA16F, true};
    frame.Bind(Id::ForwardColor, color);
    frame.Bind(Id::OutlinedColor, color);
    frame.Bind(Id::ForwardDepth, {target.GetDepthTexture(), target.GetFramebufferId(),
        target.GetDepthTexture(), 64, 48, ResourceFormat::Depth24Stencil8, true});

    auto forward = BuildPassResourceContract(RenderPassType::Forward, {false, false, false});
    // 这里只测试附件命令，省去场景反射输入；保留标准 Forward 的输出声明。
    forward.inputCount = 1;
    PassResources produce(frame, forward);
    produce.Validate();
    target.Bind();
    glClearColor(0.3f, 0.5f, 0.7f, 1.0f);
    glClearDepth(0.25);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 1, 1);
    produce.BeginTarget(Id::ForwardColor);
    GLfloat pixel[4]{};
    GLfloat depth = 0;
    glReadPixels(63, 47, 1, 1, GL_RGBA, GL_FLOAT, pixel);
    glReadPixels(63, 47, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    Require(pixel[0] == 0.0f && pixel[1] == 0.0f && pixel[2] == 0.0f && pixel[3] == 1.0f &&
        std::abs(depth - 1.0f) < 1.0e-5f,
        "Clear actions must clear whole attachments despite previous write masks and scissor.");
    GLboolean colorMask[4]{};
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    Require(!colorMask[0] && !colorMask[1] && !colorMask[2] && !colorMask[3] &&
        !depthMask && glIsEnabled(GL_SCISSOR_TEST), "Clear must restore write masks and scissor state.");

    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.3f, 0.5f, 0.7f, 1.0f);
    glClearDepth(0.25);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    produce.EndTarget(Id::ForwardColor, false);
    produce.Publish(Id::ForwardColor);
    produce.Publish(Id::ForwardDepth);
    produce.Complete();
    const auto outline = BuildPassResourceContract(RenderPassType::Outline, {false, false, false});
    PassResources preserve(frame, outline);
    preserve.Validate();
    preserve.BeginTarget(Id::OutlinedColor);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);
    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    Require(std::abs(pixel[0] - 0.3f) < 0.001f && std::abs(pixel[1] - 0.5f) < 0.001f &&
        std::abs(pixel[2] - 0.7f) < 0.001f && std::abs(depth - 0.25f) < 1.0e-5f,
        "Preserve actions must retain existing HDR color and depth.");
    preserve.EndTarget(Id::OutlinedColor, false);
    preserve.Publish(Id::OutlinedColor);
    preserve.Complete();
    glClearDepth(1.0);
    Require(glGetError() == GL_NO_ERROR, "Attachment contract operations must not cause GL errors.");
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

    TestPassAttachmentActions();
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "FramebufferTests passed." << std::endl;
    return EXIT_SUCCESS;
}
