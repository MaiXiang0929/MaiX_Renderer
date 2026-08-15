// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glad/glad.h>

#include "Renderer/Pipeline/RenderPass.h"

struct PassSubmissionStats
{
    std::size_t drawCalls = 0;
    std::size_t instancedDrawCalls = 0;
    std::size_t submittedInstances = 0;
    std::size_t shaderBindRequests = 0;
    std::size_t shaderChanges = 0;
    std::size_t materialBindRequests = 0;
    std::size_t materialChanges = 0;
    std::size_t meshBindRequests = 0;
    std::size_t meshChanges = 0;
    std::size_t textureBindRequests = 0;
    std::size_t textureChanges = 0;

    bool operator==(const PassSubmissionStats& other) const;
};

struct RenderSubmissionSnapshot
{
    static constexpr std::size_t PassCount = 8;
    std::array<PassSubmissionStats, PassCount> passes{};
    bool valid = false;
};

/// @brief 记录 CPU 向 OpenGL 提交的绑定请求，并统计相邻请求中的真实状态变化。
class RenderSubmissionStats
{
public:
    static RenderSubmissionStats& Get();

    void BeginFrame();
    void BeginPass(RenderPassType type);
    void EndPass();
    void EndFrame();

    void RecordShaderBind(GLuint program);
    void RecordMaterialBind(const void* material);
    void RecordMeshDraw(GLuint vao);
    void RecordMeshDrawInstanced(GLuint vao, std::size_t instanceCount);
    void RecordTextureBind(GLenum target, unsigned int unit, GLuint texture);
    void RecordDrawCall();

    /// @brief 返回最近一个完整帧的 CPU 提交统计，只读面板不参与统计生命周期。
    RenderSubmissionSnapshot GetSnapshot() const;

private:
    static constexpr std::size_t PassCount = RenderSubmissionSnapshot::PassCount;
    static constexpr std::size_t TextureUnitCount = 16;
    static constexpr std::size_t TextureTargetCount = 2;

    struct TextureBinding
    {
        GLuint texture = 0;
        bool known = false;
    };

    std::array<PassSubmissionStats, PassCount> m_Current{};
    std::array<PassSubmissionStats, PassCount> m_Previous{};
    std::array<
        std::array<TextureBinding, TextureTargetCount>,
        TextureUnitCount> m_TextureBindings{};
    RenderPassType m_CurrentPass = RenderPassType::Shadow;
    GLuint m_LastProgram = 0;
    GLuint m_LastVao = 0;
    const void* m_LastMaterial = nullptr;
    bool m_PassActive = false;
    bool m_HasPrevious = false;
};
