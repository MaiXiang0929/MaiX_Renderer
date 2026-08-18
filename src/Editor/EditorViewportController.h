// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include <imgui.h>

#include "ImGuizmo.h"
#include "Editor/EditorSelection.h"

class Camera;
struct EditableLight;
struct EditableModel;
class Renderer;

enum class EditorPointerButton
{
    Left,
    Middle,
    Right
};

class EditorViewportController
{
public:
    void SetButtonState(
        EditorPointerButton button,
        bool pressed,
        bool altDown,
        double x,
        double y);
    void CancelPointerInput();
    void ProcessPointerMove(
        double x,
        double y,
        bool altDown,
        float viewportHeight,
        Camera& camera,
        float* unhandledDeltaX = nullptr,
        float* unhandledDeltaY = nullptr);
    void ProcessScroll(float yOffset, Camera& camera);
    void SetOperation(ImGuizmo::OPERATION operation);
    void ToggleSpace();
    void FocusSelection(
        Camera& camera,
        const EditorSelection& selection,
        const EditableModel& model,
        const std::vector<EditableLight>& lights) const;

    bool IsLeftDown() const { return m_LeftDown; }
    ImGuizmo::OPERATION GetOperation() const { return m_Operation; }
    ImGuizmo::MODE GetMode() const { return m_Mode; }

    void Draw(
        Camera& camera,
        EditorSelection& selection,
        EditableModel& model,
        std::vector<EditableLight>& lights,
        Renderer& renderer,
        unsigned int framebufferWidth,
        unsigned int framebufferHeight,
        int windowWidth,
        int windowHeight);

private:
    bool m_LeftDown = false;
    bool m_MiddleDown = false;
    bool m_RightDown = false;
    bool m_LeftPressUsedAlt = false;
    bool m_PendingSelection = false;
    double m_LastX = 0.0;
    double m_LastY = 0.0;
    double m_LeftPressX = 0.0;
    double m_LeftPressY = 0.0;
    float m_LeftDragDistanceSquared = 0.0f;
    double m_SelectionX = 0.0;
    double m_SelectionY = 0.0;
    ImGuizmo::OPERATION m_Operation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_Mode = ImGuizmo::WORLD;
};
