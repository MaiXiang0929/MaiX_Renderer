// SPDX-License-Identifier: MIT
/// @file Camera.h
/// @brief 摄像机类的头文件
/// @details 该文件声明了 Camera 类的核心功能，包括视图矩阵和投影矩阵的计算，以及鼠标交互处理。
/// @author MaiX
/// @date 2026-08-02


#pragma once

#include "cyMatrix.h"
#include "cyVector.h"


class Camera
{
public:
	Camera(
		cy::Vec3f target = cy::Vec3f(0, 0, 0),
		float distance = 50.0f
	);

	~Camera() = default;

	// Input
	void ProcessMouseOrbit(float deltaX, float deltaY);
	void ProcessMousePan(float deltaX, float deltaY, float viewportHeight);
	void ProcessMouseZoom(float deltaY);


	// Settings
	void SetAspectRatio(float aspect);
	void ToggleProjectionMode();
	void SetTarget(const cy::Vec3f& target);
	void SetDistance(float distance);
	void SetClipPlanes(float nearPlane, float farPlane);
	void FocusBounds(const cy::Vec3f& center, float radius);


	// Output
	cy::Matrix4f GetViewMatrix() const;
	cy::Matrix4f GetProjectionMatrix() const;

	cy::Vec3f GetPosition() const;
	cy::Vec3f GetTarget() const { return m_Target; }
	float GetDistance() const { return m_Distance; }
	bool IsPerspective() const;


private:

	cy::Vec3f m_Target;

	float m_Distance;

	float m_Pitch = 0.0f;
	float m_Yaw = 0.0f;

	bool m_IsPerspective = true;

	float m_AspectRatio = 16.0f / 9.0f;

	float m_FovY = 45.0f * 3.14159f / 180.0f;

	float m_NearPlane = 0.1f;

	float m_FarPlane = 1000.0f;

	cy::Matrix4f OrthoMatrix(
		float left,
		float right,
		float bottom,
		float top,
		float nearVal,
		float farVal
	) const;
};
