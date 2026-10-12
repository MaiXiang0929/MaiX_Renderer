// SPDX-License-Identifier: MIT
#pragma once

#include <glad/glad.h>

/// @brief 缓存 Pass 内当前 VAO，避免共享 Mesh 的相邻 Draw 重复绑定。
class OpenGLStateCache
{
public:
    static OpenGLStateCache& Get();

    /// Pass 边界可能包含未经过缓存的 OpenGL 调用，因此必须使缓存失效。
    void Invalidate();
    void BindVertexArray(GLuint vao);

private:
    GLuint m_VertexArray = 0;
    bool m_VertexArrayKnown = false;
};
