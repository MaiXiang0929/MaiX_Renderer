// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/RenderSceneBridge.h"

#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Runtime/Engine/SceneState.h"
#include <algorithm>

bool RegisterSceneActors(SceneState &scene)
{
    for (auto &model : scene.models)
    {
        if (!model.actor.IsValid())
            model.actor = scene.world.Create(model.name, model.transform, ActorKind::Model, model.id.value);
        const auto *actor = scene.world.Find(model.actor);
        if (!actor || actor->kind != ActorKind::Model || actor->componentId != model.id.value)
            return false;
    }
    for (auto &light : scene.lights)
    {
        if (!light.actor.IsValid())
            light.actor = scene.world.Create(light.name, light.transform, ActorKind::Light, light.proxy.id);
        const auto *actor = scene.world.Find(light.actor);
        if (!actor || actor->kind != ActorKind::Light || actor->componentId != light.proxy.id)
            return false;
    }
    if (scene.camera.id.IsValid())
    {
        if (!scene.camera.actor.IsValid())
            scene.camera.actor = scene.world.Create(scene.camera.name, scene.camera.transform,
                                                    ActorKind::Camera, scene.camera.id.value);
        const auto *actor = scene.world.Find(scene.camera.actor);
        if (!actor || actor->kind != ActorKind::Camera || actor->componentId != scene.camera.id.value)
            return false;
    }
    return true;
}

void SynchronizeSceneActors(SceneState &scene, Renderer &renderer)
{
    auto changed = [](const Transform &a, const Transform &b) {
        const auto x = a.ToMatrix(), y = b.ToMatrix();
        for (int i = 0; i < 16; ++i)
            if (x.cell[i] != y.cell[i])
                return true;
        return false;
    };
    for (auto &model : scene.models)
        if (const auto *a = scene.world.Find(model.actor))
        {
            const bool update = changed(model.transform, a->world);
            const bool visible = scene.world.IsVisible(a->id);
            if (model.visible != visible)
                for (const auto &section : model.sections)
                    renderer.UpdatePrimitiveVisibility(section.primitiveId, visible);
            model.visible = visible;
            model.name = a->name;
            model.transform = a->world;
            if (update)
                ApplySceneModelTransform(model, renderer);
        }
    for (auto &light : scene.lights)
        if (const auto *a = scene.world.Find(light.actor))
        {
            const bool visible = scene.world.IsVisible(a->id);
            const bool update = changed(light.transform, a->world) || light.proxy.visible != visible;
            light.proxy.visible = visible;
            light.name = a->name;
            light.transform = a->world;
            if (update)
                ApplySceneLightTransform(light, renderer);
        }
    if (const auto *a = scene.world.Find(scene.camera.actor))
    {
        scene.camera.name = a->name;
        scene.camera.visible = scene.world.IsVisible(a->id);
        scene.camera.transform = a->world;
    }
}

bool DestroySceneActor(SceneState &scene, Renderer &renderer, ActorId actor)
{
    const auto removed = scene.world.Subtree(actor);
    if (removed.empty())
        return false;
    auto contains = [&](ActorId id) {
        return std::find(removed.begin(), removed.end(), id) != removed.end();
    };
    // Release every proxy before shared GPU resources, including invisible children.
    for (auto &model : scene.models)
        if (contains(model.actor))
        {
            for (const auto &section : model.sections)
                renderer.RemovePrimitive(section.primitiveId);
            model.sections.clear();
        }
    for (const auto &light : scene.lights)
        if (contains(light.actor))
            renderer.RemoveLight(light.proxy.id);
    for (const auto &model : scene.models)
        if (contains(model.actor))
            for (auto mesh : model.meshes)
                renderer.DestroyMesh(mesh);
    for (int instances = 1; instances >= 0; --instances)
        for (const auto &model : scene.models)
            if (contains(model.actor))
                for (auto material : model.materials)
                    if (renderer.IsMaterialInstance(material) == (instances != 0))
                        renderer.DestroyMaterial(material);
    for (const auto &model : scene.models)
        if (contains(model.actor) && scene.defaultModelId == model.id)
            scene.defaultModelId = {};
    for (const auto &light : scene.lights)
        if (contains(light.actor) && scene.mainLightId == light.proxy.id)
            scene.mainLightId = InvalidLightId;
    scene.models.erase(std::remove_if(scene.models.begin(), scene.models.end(),
                                      [&](const SceneModel &m) { return contains(m.actor); }),
                       scene.models.end());
    scene.lights.erase(std::remove_if(scene.lights.begin(), scene.lights.end(),
                                      [&](const SceneLight &l) { return contains(l.actor); }),
                       scene.lights.end());
    if (contains(scene.camera.actor))
        scene.camera = SceneCamera{};
    return scene.world.Destroy(actor);
}

void ApplySceneModelTransform(SceneModel &model, Renderer &renderer)
{
    const cy::Matrix4f root = model.transform.ToMatrix();
    for (const SceneModelSection &section : model.sections)
    {
        renderer.UpdatePrimitiveTransform(section.primitiveId, root * section.localTransform);
    }
}

void DestroySceneModelResources(SceneModel &model, Renderer &renderer)
{
    for (const SceneModelSection &section : model.sections)
        renderer.RemovePrimitive(section.primitiveId);
    for (const MeshHandle mesh : model.meshes)
        renderer.DestroyMesh(mesh);
    // 模型可能先记录父材质后记录实例；按依赖顺序销毁，不能依赖数组顺序。
    for (int instances = 1; instances >= 0; --instances)
        for (const MaterialHandle material : model.materials)
        {
            if (renderer.IsMaterialInstance(material) == (instances != 0))
                renderer.DestroyMaterial(material);
        }
    model.sections.clear();
    model.meshes.clear();
    model.materials.clear();
}

void ApplySceneLightTransform(SceneLight &light, Renderer &renderer)
{
    light.proxy.position = light.transform.position;
    if (light.proxy.type == LightType::Directional || light.proxy.type == LightType::Spot)
    {
        Transform rigid = light.transform;
        rigid.position = cy::Vec3f(0.0f);
        rigid.scale = cy::Vec3f(1);
        const auto direction = rigid.ToMatrix() * cy::Vec4f(0, 0, -1, 0);
        light.proxy.direction = cy::Vec3f(direction.x, direction.y, direction.z).GetNormalized();
    }
    renderer.UpdateLight(light.proxy.id, light.proxy);
}
