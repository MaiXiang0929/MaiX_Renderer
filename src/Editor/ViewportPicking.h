// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "cyMatrix.h"
#include "cyVector.h"
#include "Editor/EditorSelection.h"

struct EditableLight;
struct EditableModel;

struct EditorPickResult
{
    EditorSelectionType type = EditorSelectionType::None;
    unsigned int lightId = ~0u;
};

struct WorldRay
{
    cy::Vec3f origin;
    cy::Vec3f direction{ 0.0f, 0.0f, -1.0f };
};

WorldRay BuildWorldRay(
    float framebufferX,
    float framebufferY,
    float framebufferWidth,
    float framebufferHeight,
    const cy::Matrix4f& projection,
    const cy::Matrix4f& view);

bool IntersectRaySphere(
    const WorldRay& ray,
    const cy::Vec3f& center,
    float radius,
    float& distance);

bool HitTestEditableModel(
    const WorldRay& ray,
    const EditableModel& model,
    float& distance);

bool HitTestLightIcon(
    float framebufferX,
    float framebufferY,
    float framebufferWidth,
    float framebufferHeight,
    const cy::Matrix4f& viewProjection,
    const cy::Vec3f& worldPosition,
    float radiusPixels,
    float& depth);

EditorPickResult PickEditorObject(
    float framebufferX,
    float framebufferY,
    float framebufferWidth,
    float framebufferHeight,
    const cy::Matrix4f& projection,
    const cy::Matrix4f& view,
    const EditableModel& model,
    const std::vector<EditableLight>& lights);
