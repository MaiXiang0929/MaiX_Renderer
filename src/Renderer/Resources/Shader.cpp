// SPDX-License-Identifier: MIT
/// @file Shader.cpp
/// @brief Shader 类的实现文件
/// @details 该文件实现了 Shader 类的核心功能，包括加载、编译、绑定着色器程序，以及设置 uniform 变量。
/// @author MaiX
/// @date 2026-08-02


#include "Shader.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include "Renderer/Diagnostics/RenderSubmissionStats.h"

Shader::Shader()
{

}

Shader::~Shader()
{
	if (m_ProgramID != 0)
	{
		glDeleteProgram(m_ProgramID);
		m_ProgramID = 0;
	}
}

bool Shader::Load(
	const std::string& vertexPath,
	const std::string& fragmentPath
)
{
	std::string vertexCode = ReadFile(vertexPath);
	std::string fragmentCode = ReadFile(fragmentPath);
	if (vertexCode.empty() || fragmentCode.empty())
	{
		return false;
	}

	GLuint vertexShader = CompileShader(
		GL_VERTEX_SHADER,
		vertexCode.c_str()
	);

	GLuint fragmentShader = CompileShader(
		GL_FRAGMENT_SHADER,
		fragmentCode.c_str()
	);

	if(vertexShader == 0 || fragmentShader == 0)
	{
		return false;
	}

	GLuint program = glCreateProgram();

	glAttachShader(
		program,
		vertexShader
	);

	glAttachShader(
		program,
		fragmentShader
	);

	glLinkProgram(program);

	// 检查链接错误
	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(program, 512, nullptr, infoLog);
		std::cerr << "[Shader Error] Linking failed: " << infoLog << std::endl;

		// 链接失败时 program 不可用，两个独立 Shader 对象也不再需要。
		glDeleteProgram(program);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return false;
	}

	glDeleteShader(vertexShader);

	glDeleteShader(fragmentShader);

	// 热重载成功后再删除旧程序，避免加载失败时丢失当前可用的着色器。
	if (m_ProgramID != 0)
	{
		glDeleteProgram(m_ProgramID);
	}
	m_ProgramID = program;

	return true;
}

bool Shader::LoadTessellation(
	const std::string& vertexPath,
	const std::string& tessControlPath,
	const std::string& tessEvalPath,
	const std::string& fragmentPath,
	const std::string& geometryPath)
{
	const std::string sources[] = {
		ReadFile(vertexPath), ReadFile(tessControlPath), ReadFile(tessEvalPath),
		ReadFile(fragmentPath), geometryPath.empty() ? std::string() : ReadFile(geometryPath)
	};
	for (int i = 0; i < 4; ++i) if (sources[i].empty()) return false;

	std::vector<GLuint> shaders;
	const GLenum types[] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER,
		GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER };
	for (int i = 0; i < (geometryPath.empty() ? 4 : 5); ++i) {
		GLuint shader = CompileShader(types[i], sources[i].c_str());
		if (shader == 0) {
			for (GLuint s : shaders) glDeleteShader(s);
			return false;
		}
		shaders.push_back(shader);
	}
	return LinkProgram(shaders);
}

bool Shader::LinkProgram(const std::vector<GLuint>& shaders)
{
	GLuint program = glCreateProgram();
	for (GLuint shader : shaders) glAttachShader(program, shader);
	glLinkProgram(program);
	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[1024];
		glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "[Shader Error] Linking failed: " << infoLog << std::endl;
		glDeleteProgram(program);
		for (GLuint shader : shaders) glDeleteShader(shader);
		return false;
	}
	for (GLuint shader : shaders) glDeleteShader(shader);
	if (m_ProgramID != 0) glDeleteProgram(m_ProgramID);
	m_ProgramID = program;
	return true;
}

void Shader::Bind() const
{
	RenderSubmissionStats::Get().RecordShaderBind(m_ProgramID);
	glUseProgram(m_ProgramID);
}

void Shader::Unbind() const
{
	glUseProgram(0);
}

GLuint Shader::GetProgramID() const
{
	return m_ProgramID;
}

void Shader::SetMatrix4(
	const std::string& name,
	const float* data
)
{
	GLint location =
		glGetUniformLocation(
			m_ProgramID,
			name.c_str()
		);

	if (location == -1)
	{
		std::cerr
			<< "[Shader Warning] Uniform not found: "
			<< name
			<< std::endl;

		return;
	}

	glUniformMatrix4fv(
		location,
		1,
		GL_FALSE,
		data
	);
}

void Shader::SetVec3(
	const std::string& name,
	float x,
	float y,
	float z
)
{
	GLint location =
		glGetUniformLocation(
			m_ProgramID,
			name.c_str()
		);


	glUniform3f(
		location,
		x,
		y,
		z
	);
}

void Shader::SetInt(
	const std::string& name,
	int value
)
{
	GLint location =
		glGetUniformLocation(
			m_ProgramID,
			name.c_str()
		);


	glUniform1i(
		location,
		value
	);
}

void Shader::SetMatrix3(
	const std::string& name,
	const float* data
)
{
	const GLint location = glGetUniformLocation(m_ProgramID, name.c_str());
	if (location == -1)
	{
		std::cerr << "[Shader Warning] Uniform not found: " << name << std::endl;
		return;
	}
	glUniformMatrix3fv(location, 1, GL_FALSE, data);
}

void Shader::SetFloat(const std::string& name, float value)
{
	glUniform1f(glGetUniformLocation(m_ProgramID, name.c_str()), value);
}

std::string Shader::ReadFile(
	const std::string& path
)
{
	std::ifstream file(path);

	if (!file.is_open())
	{
		std::cerr
			<< "[Shader Error] Cannot open file: "
			<< path
			<< std::endl;

		return "";
	}

	std::stringstream ss;

	ss << file.rdbuf();

	return ss.str();
}

GLuint Shader::CompileShader(
	GLenum type,
	const char* source
)
{
	GLuint shader = glCreateShader(type);

	glShaderSource(
		shader,
		1,
		&source,
		nullptr
	);

	glCompileShader(shader);

	// 检查编译错误
	GLint success = 0;

	glGetShaderiv(
		shader,
		GL_COMPILE_STATUS,
		&success
	);

	if (!success)
	{
		char infoLog[512];

		glGetShaderInfoLog(
			shader,
			512,
			nullptr,
			infoLog
		);

		std::cerr
			<< "[Shader Error] Compilation failed: "
			<< infoLog
			<< std::endl;

		glDeleteShader(shader);

		return 0;
	}

	return shader;
}
