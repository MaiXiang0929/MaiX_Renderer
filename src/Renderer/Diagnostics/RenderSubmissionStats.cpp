// SPDX-License-Identifier: MIT
#include "RenderSubmissionStats.h"

#include <iostream>

namespace
{
std::size_t ToIndex(RenderPassType type)
{
    return static_cast<std::size_t>(type);
}

const char* ToString(RenderPassType type)
{
    switch (type)
    {
    case RenderPassType::Shadow: return "Shadow";
    case RenderPassType::Reflection: return "Reflection";
    case RenderPassType::Forward: return "Forward";
    case RenderPassType::Translucency: return "Translucency";
    case RenderPassType::EditorPrimitive: return "EditorPrimitive";
    case RenderPassType::Bloom: return "Bloom";
    case RenderPassType::PostProcess: return "PostProcess";
    case RenderPassType::Present: return "Present";
    }
    return "Unknown";
}

std::size_t TextureTargetIndex(GLenum target)
{
    return target == GL_TEXTURE_CUBE_MAP ? 1 : 0;
}
}

bool PassSubmissionStats::operator==(const PassSubmissionStats& other) const
{
    return drawCalls == other.drawCalls &&
        instancedDrawCalls == other.instancedDrawCalls &&
        submittedInstances == other.submittedInstances &&
        shaderBindRequests == other.shaderBindRequests &&
        shaderChanges == other.shaderChanges &&
        materialBindRequests == other.materialBindRequests &&
        materialChanges == other.materialChanges &&
        meshBindRequests == other.meshBindRequests &&
        meshChanges == other.meshChanges &&
        textureBindRequests == other.textureBindRequests &&
        textureChanges == other.textureChanges;
}

RenderSubmissionStats& RenderSubmissionStats::Get()
{
    static RenderSubmissionStats instance;
    return instance;
}

void RenderSubmissionStats::BeginFrame()
{
    m_Current = {};
    m_PassActive = false;
}

void RenderSubmissionStats::BeginPass(RenderPassType type)
{
    m_CurrentPass = type;
    m_LastProgram = 0;
    m_LastVao = 0;
    m_LastMaterial = nullptr;
    m_TextureBindings = {};
    m_PassActive = true;
}

void RenderSubmissionStats::EndPass()
{
    m_PassActive = false;
}

void RenderSubmissionStats::EndFrame()
{
    if (m_HasPrevious && m_Current == m_Previous)
        return;

    for (std::size_t index = 0; index < PassCount; ++index)
    {
        const RenderPassType type = static_cast<RenderPassType>(index);
        const PassSubmissionStats& stats = m_Current[index];
        std::cout
            << "[RenderStats][" << ToString(type) << "]"
            << " draws=" << stats.drawCalls
            << " instanced=" << stats.instancedDrawCalls
            << " instances=" << stats.submittedInstances
            << " shader=" << stats.shaderBindRequests
            << "/" << stats.shaderChanges
            << " material=" << stats.materialBindRequests
            << "/" << stats.materialChanges
            << " mesh=" << stats.meshBindRequests
            << "/" << stats.meshChanges
            << " texture=" << stats.textureBindRequests
            << "/" << stats.textureChanges
            << std::endl;
    }

    m_Previous = m_Current;
    m_HasPrevious = true;
}

void RenderSubmissionStats::RecordShaderBind(GLuint program)
{
    if (!m_PassActive)
        return;
    PassSubmissionStats& stats = m_Current[ToIndex(m_CurrentPass)];
    ++stats.shaderBindRequests;
    if (m_LastProgram != program)
    {
        ++stats.shaderChanges;
        m_LastProgram = program;
    }
}

void RenderSubmissionStats::RecordMaterialBind(const void* material)
{
    if (!m_PassActive)
        return;
    PassSubmissionStats& stats = m_Current[ToIndex(m_CurrentPass)];
    ++stats.materialBindRequests;
    if (m_LastMaterial != material)
    {
        ++stats.materialChanges;
        m_LastMaterial = material;
    }
}

void RenderSubmissionStats::RecordMeshDraw(GLuint vao)
{
    if (!m_PassActive)
        return;
    PassSubmissionStats& stats = m_Current[ToIndex(m_CurrentPass)];
    ++stats.drawCalls;
    ++stats.submittedInstances;
    ++stats.meshBindRequests;
    if (m_LastVao != vao)
    {
        ++stats.meshChanges;
        m_LastVao = vao;
    }
}

void RenderSubmissionStats::RecordMeshDrawInstanced(
    GLuint vao,
    std::size_t instanceCount)
{
    if (!m_PassActive || instanceCount == 0)
        return;
    PassSubmissionStats& stats = m_Current[ToIndex(m_CurrentPass)];
    ++stats.drawCalls;
    ++stats.instancedDrawCalls;
    stats.submittedInstances += instanceCount;
    ++stats.meshBindRequests;
    if (m_LastVao != vao)
    {
        ++stats.meshChanges;
        m_LastVao = vao;
    }
}

void RenderSubmissionStats::RecordTextureBind(
    GLenum target,
    unsigned int unit,
    GLuint texture)
{
    if (!m_PassActive)
        return;
    PassSubmissionStats& stats = m_Current[ToIndex(m_CurrentPass)];
    ++stats.textureBindRequests;
    if (unit >= TextureUnitCount)
    {
        ++stats.textureChanges;
        return;
    }

    TextureBinding& binding =
        m_TextureBindings[unit][TextureTargetIndex(target)];
    if (!binding.known || binding.texture != texture)
    {
        ++stats.textureChanges;
        binding.texture = texture;
        binding.known = true;
    }
}

void RenderSubmissionStats::RecordDrawCall()
{
    if (m_PassActive)
    {
        ++m_Current[ToIndex(m_CurrentPass)].drawCalls;
        ++m_Current[ToIndex(m_CurrentPass)].submittedInstances;
    }
}

RenderSubmissionSnapshot RenderSubmissionStats::GetSnapshot() const
{
    RenderSubmissionSnapshot snapshot;
    snapshot.passes = m_Previous;
    snapshot.valid = m_HasPrevious;
    return snapshot;
}
