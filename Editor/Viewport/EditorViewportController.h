// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include <imgui.h>

#include "ImGuizmo.h"
#include "Editor/Session/EditorSelection.h"

class Camera;
struct SceneLight;
struct SceneModel;
class Renderer;
struct EditorViewportRegion;

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
    void CancelPointerInput(class EditorHistory* history = nullptr, World* world = nullptr);
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
        const std::vector<SceneModel>& models,
        const std::vector<SceneLight>& lights, const SceneCamera* sceneCamera = nullptr,
        const World* world = nullptr) const;

    bool IsLeftDown() const { return m_LeftDown; }
    bool HasPointerButtonDown() const { return m_LeftDown || m_MiddleDown || m_RightDown; }
    ImGuizmo::OPERATION GetOperation() const { return m_Operation; }
    ImGuizmo::MODE GetMode() const { return m_Mode; }
    ImGuizmo::OPERATION GetEffectiveOperation(EditorSelectionType type, const SceneLight* light) const;
    void DrawToolbar(const EditorSelection& selection, const std::vector<SceneLight>& lights, bool cameraView);

    void Draw(
        Camera& camera,
        EditorSelection& selection,
        SceneState& scene,
        Renderer& renderer,
        const EditorViewportRegion& viewport, SceneCamera* sceneCamera = nullptr, bool cameraView = false,
        class EditorHistory* history = nullptr);

private:
    std::string m_TransformError;
    bool m_EditingGizmo = false;
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
