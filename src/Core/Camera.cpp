// SPDX-License-Identifier: MIT
/// @file Camera.cpp
/// @brief 摄像机类的实现文件
/// @details 该文件实现了 Camera 类的核心功能，包括视图矩阵和投影矩阵的计算，以及鼠标交互处理。
/// @author MaiX
/// @date 2026-08-02


#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float OrbitSensitivity = 0.01f;
constexpr float DollySensitivity = 0.01f;
constexpr float MinimumDistance = 0.01f;
constexpr float MaximumPitch = 89.0f * Pi / 180.0f;
}

Camera::Camera(
	cy::Vec3f target,
	float distance)
	: m_Target(target),
	m_Distance(distance)
{

}

void Camera::ProcessMouseOrbit(
	float deltaX,
	float deltaY
)
{
	m_Yaw += deltaX * OrbitSensitivity;
	m_Pitch = std::clamp(
		m_Pitch + deltaY * OrbitSensitivity,
		-MaximumPitch,
		MaximumPitch);
}

void Camera::ProcessMousePan(
	float deltaX,
	float deltaY,
	float viewportHeight)
{
	if (viewportHeight <= 0.0f)
		return;

	cy::Vec3f forward = m_Target - GetPosition();
	forward.Normalize();
	cy::Vec3f right = forward.Cross(cy::Vec3f(0.0f, 1.0f, 0.0f));
	right.Normalize();
	const cy::Vec3f up = right.Cross(forward);
	const float worldUnitsPerPixel =
		2.0f * m_Distance * std::tan(m_FovY * 0.5f) / viewportHeight;
	m_Target += right * (-deltaX * worldUnitsPerPixel) +
		up * (deltaY * worldUnitsPerPixel);
}

void Camera::ProcessMouseZoom(
	float deltaY
)
{
	m_Distance = std::max(
		MinimumDistance,
		m_Distance * std::exp(deltaY * DollySensitivity));
}


void Camera::SetAspectRatio(
	float aspect
)
{
	m_AspectRatio = aspect;
}


void Camera::ToggleProjectionMode()
{
	m_IsPerspective = !m_IsPerspective;
}


void Camera::SetTarget(
	const cy::Vec3f& target
)
{
	m_Target = target;
}


void Camera::SetDistance(
	float distance
)
{
	m_Distance = std::max(distance, MinimumDistance);
}

void Camera::SetClipPlanes(float nearPlane, float farPlane)
{
	if (nearPlane <= 0.0f || farPlane <= nearPlane)
		return;

	m_NearPlane = nearPlane;
	m_FarPlane = farPlane;
}

void Camera::FocusBounds(const cy::Vec3f& center, float radius)
{
	m_Target = center;
	if (radius <= 0.0f)
		return;

	const float halfVerticalFov = m_FovY * 0.5f;
	const float halfHorizontalFov = std::atan(
		std::tan(halfVerticalFov) * m_AspectRatio);
	const float limitingHalfFov = std::min(
		halfVerticalFov, halfHorizontalFov);
	m_Distance = std::max(
		MinimumDistance,
		radius * 1.15f / std::sin(limitingHalfFov));
}

cy::Matrix4f Camera::GetViewMatrix() const
{
	return cy::Matrix4f::View(
		GetPosition(), m_Target, cy::Vec3f(0.0f, 1.0f, 0.0f));
}


cy::Matrix4f Camera::GetProjectionMatrix() const
{
	cy::Matrix4f proj;

	if (m_IsPerspective)
	{
		proj.SetPerspective(
			m_FovY,
			m_AspectRatio,
			m_NearPlane,
			m_FarPlane
		);
	}
	else
	{
		float halfHeight = m_Distance * tan(m_FovY / 2.0f);
		float halfWidth = halfHeight * m_AspectRatio;
		proj = OrthoMatrix(
			-halfWidth, halfWidth,
			-halfHeight, halfHeight,
			m_NearPlane, m_FarPlane
		);
	}

	return proj;
}


cy::Vec3f Camera::GetPosition() const
{
	const float cosPitch = std::cos(m_Pitch);
	const cy::Vec3f offset(
		std::sin(m_Yaw) * cosPitch,
		std::sin(m_Pitch),
		std::cos(m_Yaw) * cosPitch);
	return m_Target + offset * m_Distance;
}


bool Camera::IsPerspective() const
{
	return m_IsPerspective;
}


cy::Matrix4f Camera::OrthoMatrix(
	float left, float right,
	float bottom, float top,
	float nearVal, float farVal
) const
{
	cy::Matrix4f m;

	m.Zero();

	m.cell[0] = 2.0f / (right - left);
	m.cell[5] = 2.0f / (top - bottom);
	m.cell[10] = -2.0f / (farVal - nearVal);
	m.cell[12] = -(right + left) / (right - left);
	m.cell[13] = -(top + bottom) / (top - bottom);
	m.cell[14] = -(farVal + nearVal) / (farVal - nearVal);
	m.cell[15] = 1.0f;

	return m;
}
