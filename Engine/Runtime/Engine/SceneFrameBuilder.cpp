// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/SceneFrameBuilder.h"
#include "Engine/Core/Math/CameraClipRange.h"
#include "Engine/Rendering/Renderer/View/DirectionalShadow.h"
#include <algorithm>
#include <cmath>
#include <limits>

RenderFrameData BuildSceneFrame(SceneState& scene, Camera& camera,
    unsigned int width, unsigned int height, bool sceneCameraView,
    const SceneFrameSettings& settings)
{
    // 共用矩阵
    cy::Matrix4f projMatrix = camera.GetProjectionMatrix();
    cy::Matrix4f viewMatrix = sceneCameraView ? scene.camera.GetViewMatrix() : camera.GetViewMatrix();

    const PrimitiveBounds sceneBounds = GetSceneWorldBounds(scene.models);
    const cy::Vec3f sceneCenter = sceneBounds.radius > 0.0f
        ? sceneBounds.center
        : cy::Vec3f(0.0f, 0.0f, 0.0f);
    const float sceneRadius = std::max(
        std::max(scene.sceneRadius, sceneBounds.radius), 0.001f);

    SceneLight* editableMainLight = FindSceneLight(scene.lights, scene.mainLightId);
    float shadowPadding = 0;
    if (settings.tessellationEnabled)
        for (const auto& model : scene.models)
            for (const auto& section : model.sections)
            {
                PrimitiveBounds displacementBounds;
                displacementBounds.radius = settings.displacementScale;
                shadowPadding = std::max(shadowPadding, TransformBounds(displacementBounds,
                    model.transform.ToMatrix()*section.localTransform).radius);
            }
    const cy::Matrix4f lightVP = FitDirectionalShadow(sceneBounds,
        editableMainLight ? editableMainLight->proxy.direction : cy::Vec3f(0,-1,0),
        shadowPadding);
    // 反射视图仍由 Engine frame builder 根据场景地面位置计算，Pass 只消费结果。
    float groundY = scene.groundY;
    if (scene.models.size() == 1 && scene.models.front().id == scene.defaultModelId)
    {
        const SceneModel& model = scene.models.front();
        const cy::Matrix4f root = model.transform.ToMatrix();
        groundY = std::numeric_limits<float>::max();
        for (float x : {scene.defaultBoundsMin.x, scene.defaultBoundsMax.x})
            for (float y : {scene.defaultBoundsMin.y, scene.defaultBoundsMax.y})
                for (float z : {scene.defaultBoundsMin.z, scene.defaultBoundsMax.z})
                {
                    const cy::Vec4f corner = root * cy::Vec4f(x, y, z, 1.0f);
                    groundY = std::min(groundY, corner.y);
                }
        const float maxScale = std::max({std::abs(model.transform.scale.x),
            std::abs(model.transform.scale.y), std::abs(model.transform.scale.z)});
        groundY -= scene.modelDiameter * 0.02f * maxScale;
    }
    else
    {
        bool hasModelBounds = false;
        for (const SceneModel& model : scene.models)
        {
            const PrimitiveBounds bounds = model.GetWorldBounds();
            if (bounds.radius <= 0.0f)
                continue;
            const float bottom = bounds.center.y - bounds.radius;
            groundY = hasModelBounds ? std::min(groundY, bottom) : bottom;
            hasModelBounds = true;
        }
    }
    const cy::Matrix4f reflectMatrix =
        cy::Matrix4f::Translation(cy::Vec3f(0.0f, groundY, 0.0f)) *
        cy::Matrix4f::Scale(1.0f, -1.0f, 1.0f) *
        cy::Matrix4f::Translation(cy::Vec3f(0.0f, -groundY, 0.0f));
    const cy::Matrix4f reflectView = viewMatrix * reflectMatrix;

    // 相机世界位置（用于地面着色器视线方向计算）
    const cy::Vec3f cameraWorldPos = sceneCameraView ? scene.camera.transform.position : camera.GetPosition();

    const float groundSize = std::max(
        scene.modelDiameter * 2.0f, sceneRadius * 2.0f);
    const cy::Vec3f groundCenter(sceneCenter.x, groundY, sceneCenter.z);
    const cy::Matrix4f groundModel =
        cy::Matrix4f::Translation(groundCenter) *
        cy::Matrix4f::Scale(groundSize, 1.0f, groundSize);

    // Main and reflection share a projection. Fit both depth ranges before
    // creating it, including the ground quad and the editor's light cones.
    CameraClipRange clipRange;
    for (const SceneModel& model : scene.models)
    {
        const cy::Matrix4f root = model.transform.ToMatrix();
        for (const SceneModelSection& section : model.sections)
        {
            if (section.localBounds.radius <= 0.0f)
                continue;
            PrimitiveBounds localBounds = section.localBounds;
            if (settings.tessellationEnabled)
                localBounds.radius += settings.displacementScale;
            const PrimitiveBounds bounds = TransformBounds(localBounds, root * section.localTransform);
            clipRange.IncludeSphere(viewMatrix, bounds.center, bounds.radius);
            if (scene.reflectionGroundEnabled) clipRange.IncludeSphere(reflectView, bounds.center, bounds.radius);
        }
    }
    if (scene.reflectionGroundEnabled)
    {
        for (float x : {-1.0f, 1.0f})
            for (float z : {-1.0f, 1.0f})
                clipRange.IncludeSphere(viewMatrix,
                    groundCenter + cy::Vec3f(x * groundSize, 0.0f, z * groundSize), 0.0f);
    }
    for (const SceneLight& light : scene.lights)
    {
        if (light.proxy.type == LightType::Directional)
            continue;
        clipRange.IncludeSphere(viewMatrix, light.transform.position, 0.0f);
        if (light.proxy.type == LightType::Spot)
        {
            const float range = light.proxy.range;
            const float coneRadius = range * std::tan(light.proxy.outerConeAngle);
            const float radius = std::sqrt(range * range * 0.25f + coneRadius * coneRadius);
            clipRange.IncludeSphere(viewMatrix, light.transform.position +
                light.proxy.direction * (range * 0.5f), radius);
        }
    }
    if (!sceneCameraView && settings.editorPrimitivesEnabled)
    {
        const float previewHeight = std::tan(scene.camera.fovDegrees * 3.14159265358979323846f / 360);
        const float previewAspect = width > 0 && height > 0 ? static_cast<float>(width)/height : 1;
        clipRange.IncludeSphere(viewMatrix, scene.camera.transform.position,
            std::sqrt(1 + previewHeight*previewHeight*(1+previewAspect*previewAspect)));
    }
    if (!sceneCameraView) camera.SetClipPlanes(clipRange.Near(), clipRange.Far());
    const float aspect = width > 0 && height > 0 ? static_cast<float>(width)/height : 1.0f;
    projMatrix = sceneCameraView ? scene.camera.GetProjectionMatrix(aspect) : camera.GetProjectionMatrix();

    // Engine frame builder 只提交强类型帧数据，不再创建任何 Pass callback。
    RenderFrameData frame;
    frame.editorPrimitivesEnabled = settings.editorPrimitivesEnabled;
    frame.viewportWidth = width;
    frame.viewportHeight = height;
    frame.projection = projMatrix;
    frame.view = viewMatrix;
    frame.lightVP = lightVP;
    frame.shadowsEnabled = editableMainLight && editableMainLight->proxy.castsShadow && sceneBounds.radius > 0;
    frame.reflectionGroundEnabled = scene.reflectionGroundEnabled;
    frame.shadowLightId = frame.shadowsEnabled ? scene.mainLightId : InvalidLightId;
    frame.keyLightId = scene.mainLightId;
    frame.reflectionView = reflectView;
    frame.groundModel = groundModel;
    frame.groundMvp = projMatrix * viewMatrix * groundModel;
    frame.reflectionVP = projMatrix * reflectView;
    frame.cameraWorldPosition = cameraWorldPos;
    return frame;
}
