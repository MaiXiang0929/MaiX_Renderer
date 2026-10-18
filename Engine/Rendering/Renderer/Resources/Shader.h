// SPDX-License-Identifier: MIT
/// @file Shader.h
/// @brief Shader 类的头文件
/// @details 该文件声明了 Shader 类的核心功能，包括加载、编译、绑定着色器程序，以及设置 uniform 变量。
/// @author MaiX
/// @date 2026-08-02


#pragma once

#include <string>
#include <vector>
#include "Engine/Rendering/RHI/RHIDevice.h"



class Shader
{
public:
	explicit Shader(RHIDevice& device) : m_Device(device) {}

	~Shader();

	// 禁止复制
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	bool Load(
		const std::string& vertexPath,
		const std::string& fragmentPath
	);

	bool LoadTessellation(
		const std::string& vertexPath,
		const std::string& tessControlPath,
		const std::string& tessEvalPath,
		const std::string& fragmentPath,
		const std::string& geometryPath = {}
	);

	void Bind() const;

	void Unbind() const;

	RHIProgramHandle GetProgramID() const;
    RHIDevice& Device() const { return m_Device; }

	void SetMatrix4(
		const std::string& name,
		const float* data
	);
	void SetMatrix3(
		const std::string& name,
		const float* data
	);

	void SetVec3(
		const std::string& name,
		float x,
		float y,
		float z
	);

	void SetInt(
		const std::string& name,
		int value
	);
	void SetFloat(const std::string& name, float value);

private:
    RHIDevice& m_Device;
	RHIProgramHandle m_ProgramID = 0;

private:

	std::string ReadFile(
		const std::string& path
	);

};
