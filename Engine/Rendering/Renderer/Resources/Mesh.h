// SPDX-License-Identifier: MIT
/// @file Mesh.h
/// @brief Mesh 类的声明文件
/// @details 该文件声明了 Mesh 类，包含顶点数据管理和 OpenGL 渲染相关功能。
/// @author MaiX
/// @date 2026-08-04


#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <glad/glad.h>
#include "cyMatrix.h"
#include "cyVector.h"

/// @brief 顶点结构体，包含位置、法线和纹理坐标
struct Vertex {
	cy::Vec3f Position;
	cy::Vec3f Normal;
	cy::Vec2f TexCoord;
	cy::Vec4f Tangent;
};

class Mesh {

public:

    Mesh();

    ~Mesh();

	// 禁止复制
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void Upload(
        const std::vector<Vertex>& vertices
    );

    void Upload(
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices
    );

    void Draw() const;

    void DrawInstanced(std::size_t instanceCount) const;

    void DrawPatches() const;

    int GetVertexCount() const;

    int GetIndexCount() const;
 

private:

    GLuint m_VAO = 0;

    GLuint m_VBO = 0;

    GLuint m_EBO = 0;

    int m_VertexCount = 0;

    int m_IndexCount = 0;

private:

    /// @brief 创建 OpenGL Buffer
    void CreateBuffers();

    /// @brief 设置 Vertex Attribute Layout
    void SetupVertexAttributes();

};
