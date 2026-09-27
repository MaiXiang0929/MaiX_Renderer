// SPDX-License-Identifier: MIT
#include "EditorViewportController.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "Core/Camera.h"
#include "Editor/EditableLight.h"
#include "Editor/EditableModel.h"
#include "Editor/ViewportPicking.h"
#include "Editor/EditorWorkspace.h"
#include "Renderer/Core/Renderer.h"

void EditorViewportController::SetButtonState(
    EditorPointerButton button,
    bool pressed,
    bool altDown,
    double x,
    double y)
{
    m_LastX = x;
    m_LastY = y;
    if (button == EditorPointerButton::Left)
    {
        if (pressed)
        {
            m_LeftPressX = x;
            m_LeftPressY = y;
            m_LeftDragDistanceSquared = 0.0f;
            m_LeftPressUsedAlt = altDown;
        }
        else if (m_LeftDown && !m_LeftPressUsedAlt &&
                 m_LeftDragDistanceSquared <= 16.0f)
        {
            m_PendingSelection = true;
            m_SelectionX = x;
            m_SelectionY = y;
        }
        m_LeftDown = pressed;
    }
    else if (button == EditorPointerButton::Middle)
        m_MiddleDown = pressed;
    else
        m_RightDown = pressed;
}

void EditorViewportController::CancelPointerInput()
{
    m_LeftDown = false;
    m_MiddleDown = false;
    m_RightDown = false;
    m_PendingSelection = false;
}

void EditorViewportController::ProcessPointerMove(
    double x,
    double y,
    bool altDown,
    float viewportHeight,
    Camera& camera,
    float* unhandledDeltaX,
    float* unhandledDeltaY)
{
    const float deltaX = static_cast<float>(x - m_LastX);
    const float deltaY = static_cast<float>(y - m_LastY);
    m_LastX = x;
    m_LastY = y;
    if (m_LeftDown)
    {
        const float fromPressX = static_cast<float>(x - m_LeftPressX);
        const float fromPressY = static_cast<float>(y - m_LeftPressY);
        m_LeftDragDistanceSquared =
            fromPressX * fromPressX + fromPressY * fromPressY;
    }

    if (unhandledDeltaX)
        *unhandledDeltaX = deltaX;
    if (unhandledDeltaY)
        *unhandledDeltaY = deltaY;

    if (!altDown || ImGuizmo::IsUsing())
        return;
    if (m_LeftDown)
        camera.ProcessMouseOrbit(deltaX, deltaY);
    else if (m_MiddleDown)
        camera.ProcessMousePan(deltaX, deltaY, viewportHeight);
    else if (m_RightDown)
        camera.ProcessMouseZoom(deltaY);
}

void EditorViewportController::ProcessScroll(float yOffset, Camera& camera)
{
    camera.ProcessMouseZoom(-yOffset * 8.0f);
}

void EditorViewportController::SetOperation(ImGuizmo::OPERATION operation)
{
    m_Operation = operation;
}

void EditorViewportController::ToggleSpace()
{
    m_Mode = m_Mode == ImGuizmo::WORLD ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

void EditorViewportController::FocusSelection(
    Camera& camera,
    const EditorSelection& selection,
    const EditableModel& model,
    const std::vector<EditableLight>& lights) const
{
    if (selection.type == EditorSelectionType::Model)
    {
        const PrimitiveBounds bounds = model.GetWorldBounds();
        camera.FocusBounds(bounds.center, bounds.radius);
    }
    else if (selection.type == EditorSelectionType::Light)
    {
        const EditableLight* light = FindEditableLight(lights, selection.lightId);
        if (light && light->proxy.type != LightType::Directional)
            camera.FocusBounds(light->transform.position, 1.0f);
    }
}

void EditorViewportController::Draw(
    Camera& camera,
    EditorSelection& selection,
    EditableModel& model,
    std::vector<EditableLight>& lights,
    Renderer& renderer,
    const EditorViewportRegion& viewport)
{
    bool transformChanged = false;
    EditableLight* selectedLight = selection.type == EditorSelectionType::Light
        ? FindEditableLight(lights, selection.lightId)
        : nullptr;
    if (selection.type == EditorSelectionType::Light && !selectedLight)
    {
        selection.Clear();
    }
    const bool modelSelected = selection.IsModelSelected();

    ImGuizmo::SetOrthographic(!camera.IsPerspective());
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(
        viewport.min.x,
        viewport.min.y,
        viewport.size.x,
        viewport.size.y);

    const bool lightHasPosition = selectedLight &&
        selectedLight->proxy.type != LightType::Directional;
    if (modelSelected || lightHasPosition)
    {
        Transform& activeTransform = modelSelected
            ? model.transform
            : selectedLight->transform;
        cy::Matrix4f matrix = activeTransform.ToMatrix();
        const cy::Matrix4f view = camera.GetViewMatrix();
        const cy::Matrix4f projection = camera.GetProjectionMatrix();
        if (ImGuizmo::Manipulate(
                view.cell,
                projection.cell,
                modelSelected ? m_Operation : ImGuizmo::TRANSLATE,
                m_Mode,
                matrix.cell))
        {
            float translation[3];
            float rotation[3];
            float scale[3];
            ImGuizmo::DecomposeMatrixToComponents(
                matrix.cell, translation, rotation, scale);
            activeTransform.position = cy::Vec3f(
                translation[0], translation[1], translation[2]);
            if (modelSelected)
            {
                activeTransform.rotationDegrees = cy::Vec3f(
                    rotation[0], rotation[1], rotation[2]);
                activeTransform.scale = cy::Vec3f(
                    std::clamp(std::abs(scale[0]), 0.001f, 1000.0f),
                    std::clamp(std::abs(scale[1]), 0.001f, 1000.0f),
                    std::clamp(std::abs(scale[2]), 0.001f, 1000.0f));
            }
            transformChanged = true;
        }
    }

    if (m_PendingSelection)
    {
        if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
            viewport.Contains(m_SelectionX, m_SelectionY) &&
            viewport.pixelWidth > 0 && viewport.pixelHeight > 0)
        {
            // GLFW 鼠标坐标属于窗口；先减去视口图像左上角，再换算到 GPU 像素。
            const float framebufferX = static_cast<float>(
                (m_SelectionX - viewport.min.x) *
                viewport.pixelWidth / viewport.size.x);
            const float framebufferY = static_cast<float>(
                (m_SelectionY - viewport.min.y) *
                viewport.pixelHeight / viewport.size.y);
            const EditorPickResult pick = PickEditorObject(
                framebufferX,
                framebufferY,
                static_cast<float>(viewport.pixelWidth),
                static_cast<float>(viewport.pixelHeight),
                camera.GetProjectionMatrix(),
                camera.GetViewMatrix(),
                model,
                lights);
            if (pick.type == EditorSelectionType::Light)
            {
                selection.SelectLight(pick.lightId);
            }
            else
            {
                if (pick.type == EditorSelectionType::Model)
                    selection.SelectModel();
                else
                    selection.Clear();
            }
        }
        m_PendingSelection = false;
    }

    if (transformChanged && modelSelected)
    {
        model.transform.scale.x = std::clamp(
            std::abs(model.transform.scale.x), 0.001f, 1000.0f);
        model.transform.scale.y = std::clamp(
            std::abs(model.transform.scale.y), 0.001f, 1000.0f);
        model.transform.scale.z = std::clamp(
            std::abs(model.transform.scale.z), 0.001f, 1000.0f);
        ApplyEditableModelTransform(model, renderer);
    }
    else if (transformChanged && selectedLight)
    {
        ApplyEditableLightTransform(*selectedLight, renderer);
    }
}
