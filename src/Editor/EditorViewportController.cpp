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
    const std::vector<EditableModel>& models,
    const std::vector<EditableLight>& lights, const EditableCamera* sceneCamera) const
{
    if (selection.type == EditorSelectionType::Model)
    {
        const EditableModel* model = FindEditableModel(models, selection.modelId);
        if (model)
        {
            const PrimitiveBounds bounds = model->GetWorldBounds();
            camera.FocusBounds(bounds.center, bounds.radius);
        }
    }
    else if (sceneCamera && selection.IsCameraSelected(sceneCamera->id))
        camera.FocusBounds(sceneCamera->transform.position, 1.0f);
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
    std::vector<EditableModel>& models,
    std::vector<EditableLight>& lights,
    Renderer& renderer,
    const EditorViewportRegion& viewport, EditableCamera* sceneCamera, bool cameraView)
{
    if (viewport.pixelWidth == 0 || viewport.pixelHeight == 0 || viewport.size.x <= 0 || viewport.size.y <= 0)
        return;
    if (cameraView)
        ImGui::GetWindowDrawList()->AddText(ImVec2(viewport.min.x+12,viewport.min.y+12),
            IM_COL32(255,230,160,255), "Camera View - Numpad 0: Editor View");
    bool transformChanged = false;
    EditableLight* selectedLight = selection.type == EditorSelectionType::Light
        ? FindEditableLight(lights, selection.lightId)
        : nullptr;
    if (selection.type == EditorSelectionType::Light && !selectedLight)
    {
        selection.Clear();
    }
    EditableModel* selectedModel = selection.type == EditorSelectionType::Model
        ? FindEditableModel(models, selection.modelId)
        : nullptr;
    if (selection.type == EditorSelectionType::Model && !selectedModel)
        selection.Clear();
    const bool modelSelected = selectedModel != nullptr;
    EditableCamera* selectedCamera = sceneCamera && selection.IsCameraSelected(sceneCamera->id) ? sceneCamera : nullptr;
    const float aspect = static_cast<float>(viewport.pixelWidth) / std::max(viewport.pixelHeight, 1u);
    const auto view = cameraView && sceneCamera ? sceneCamera->GetViewMatrix() : camera.GetViewMatrix();
    const auto projection = cameraView && sceneCamera ? sceneCamera->GetProjectionMatrix(aspect) : camera.GetProjectionMatrix();

    ImGuizmo::SetOrthographic(!cameraView && !camera.IsPerspective());
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(
        viewport.min.x,
        viewport.min.y,
        viewport.size.x,
        viewport.size.y);

    const bool lightHasPosition = selectedLight &&
        selectedLight->proxy.type != LightType::Directional;
    if (modelSelected || (selectedCamera && !cameraView) || selectedLight)
    {
        Transform& activeTransform = modelSelected
            ? selectedModel->transform
            : selectedCamera ? selectedCamera->transform : selectedLight->transform;
        cy::Matrix4f matrix = activeTransform.ToMatrix();
        const auto operation = modelSelected ? m_Operation :
            selectedCamera ? (m_Operation == ImGuizmo::SCALE ? ImGuizmo::TRANSLATE : m_Operation) :
            lightHasPosition ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE;
        if (ImGuizmo::Manipulate(
                view.cell,
                projection.cell,
                operation,
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
            if (selectedCamera || selectedLight)
                activeTransform.rotationDegrees = cy::Vec3f(rotation[0],rotation[1],rotation[2]);
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

    if (sceneCamera && !cameraView && renderer.AreEditorPrimitivesEnabled())
    {
        const auto rigid = sceneCamera->GetViewMatrix().GetInverse();
        const auto vp = projection * view;
        auto project = [&](cy::Vec3f p, ImVec2& screen)
        {
            const auto world = rigid * cy::Vec4f(p.x,p.y,p.z,1);
            const auto clip = vp * world;
            if (clip.w <= 0 || clip.z < -clip.w || clip.z > clip.w) return false;
            screen = ImVec2(viewport.min.x + (clip.x/clip.w+1)*0.5f*viewport.size.x,
                viewport.min.y + (1-clip.y/clip.w)*0.5f*viewport.size.y);
            return true;
        };
        const auto color = selection.IsCameraSelected(sceneCamera->id) ? IM_COL32(255,190,50,255) : IM_COL32(170,210,255,255);
        const float height = std::tan(sceneCamera->fovDegrees*3.14159265358979323846f/360);
        const float width = height * aspect;
        const cy::Vec3f corners[] = {{-width,-height,-1},{width,-height,-1},{width,height,-1},{-width,height,-1}};
        ImVec2 origin;
        if (project({0,0,0},origin))
        {
            ImGui::GetWindowDrawList()->AddCircle(origin,8,color);
            ImGui::GetWindowDrawList()->AddText(ImVec2(origin.x+10,origin.y),color,sceneCamera->name.c_str());
        }
        for (int i=0;i<4;++i)
        {
            ImVec2 a,b;
            if (project({0,0,0},a) && project(corners[i],b)) ImGui::GetWindowDrawList()->AddLine(a,b,color);
            if (project(corners[i],a) && project(corners[(i+1)%4],b)) ImGui::GetWindowDrawList()->AddLine(a,b,color);
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
                projection,
                view,
                models,
                lights);
            float cameraDepth;
            if (sceneCamera && !cameraView && renderer.AreEditorPrimitivesEnabled() &&
                HitTestLightIcon(framebufferX,framebufferY,static_cast<float>(viewport.pixelWidth),
                    static_cast<float>(viewport.pixelHeight),projection*view,sceneCamera->transform.position,12,cameraDepth))
                selection.SelectCamera(sceneCamera->id);
            else if (pick.type == EditorSelectionType::Light)
            {
                selection.SelectLight(pick.lightId);
            }
            else
            {
                if (pick.type == EditorSelectionType::Model)
                    selection.SelectModel(pick.modelId);
                else
                    selection.Clear();
            }
        }
        m_PendingSelection = false;
    }

    if (transformChanged && modelSelected)
    {
        selectedModel->transform.scale.x = std::clamp(
            std::abs(selectedModel->transform.scale.x), 0.001f, 1000.0f);
        selectedModel->transform.scale.y = std::clamp(
            std::abs(selectedModel->transform.scale.y), 0.001f, 1000.0f);
        selectedModel->transform.scale.z = std::clamp(
            std::abs(selectedModel->transform.scale.z), 0.001f, 1000.0f);
        ApplyEditableModelTransform(*selectedModel, renderer);
    }
    else if (transformChanged && selectedLight)
    {
        ApplyEditableLightTransform(*selectedLight, renderer);
    }
}
