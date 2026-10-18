// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
Shader::~Shader()
{
    m_Device.DestroyProgram(m_ProgramID);
}
std::string Shader::ReadFile(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "[Shader] Cannot open source: " << path << '\n';
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}
bool Shader::Load(const std::string &vertex, const std::string &fragment)
{
    const auto candidate = m_Device.CreateProgram({{RHIShaderStage::Vertex, ReadFile(vertex), vertex},
                                                   {RHIShaderStage::Fragment, ReadFile(fragment), fragment}});
    if (candidate == 0)
        return false;
    m_Device.DestroyProgram(m_ProgramID);
    m_ProgramID = candidate;
    return true;
}
bool Shader::LoadTessellation(const std::string &vertex, const std::string &control,
                              const std::string &evaluation, const std::string &fragment,
                              const std::string &geometry)
{
    std::vector<RHIShaderSource> sources = {
        {RHIShaderStage::Vertex, ReadFile(vertex), vertex},
        {RHIShaderStage::TessControl, ReadFile(control), control},
        {RHIShaderStage::TessEvaluation, ReadFile(evaluation), evaluation},
        {RHIShaderStage::Fragment, ReadFile(fragment), fragment}};
    if (!geometry.empty())
        sources.push_back({RHIShaderStage::Geometry, ReadFile(geometry), geometry});
    const auto candidate = m_Device.CreateProgram(sources);
    if (candidate == 0)
        return false;
    m_Device.DestroyProgram(m_ProgramID);
    m_ProgramID = candidate;
    return true;
}
void Shader::Bind() const
{
    m_Device.Commands().BindProgram(m_ProgramID);
}
void Shader::Unbind() const
{
    m_Device.Commands().BindProgram({});
}
RHIProgramHandle Shader::GetProgramID() const
{
    return m_ProgramID;
}
void Shader::SetInt(const std::string &name, int value)
{
    m_Device.Commands().SetInt(m_ProgramID, name, value);
}
void Shader::SetFloat(const std::string &name, float value)
{
    m_Device.Commands().SetFloats(m_ProgramID, name, &value, 1);
}
void Shader::SetVec3(const std::string &name, float x, float y, float z)
{
    const float values[] = {x, y, z};
    m_Device.Commands().SetFloats(m_ProgramID, name, values, 3);
}
void Shader::SetMatrix3(const std::string &name, const float *data)
{
    m_Device.Commands().SetFloats(m_ProgramID, name, data, 9);
}
void Shader::SetMatrix4(const std::string &name, const float *data)
{
    m_Device.Commands().SetFloats(m_ProgramID, name, data, 16);
}
