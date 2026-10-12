// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Engine/RenderSceneBridge.h"

#include "Engine/Rendering/Renderer/Core/Renderer.h"

void ApplySceneModelTransform(SceneModel& model, Renderer& renderer)
{
    const cy::Matrix4f root = model.transform.ToMatrix();
    for (const SceneModelSection& section : model.sections)
    {
        renderer.UpdatePrimitiveTransform(
            section.primitiveId, root * section.localTransform);
    }
}

void DestroySceneModelResources(SceneModel& model, Renderer& renderer)
{
    for (const SceneModelSection& section : model.sections)
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


void ApplySceneLightTransform(
    SceneLight& light,
    Renderer& renderer)
{
    light.proxy.position = light.transform.position;
    if (light.proxy.type == LightType::Directional)
    {
        Transform rigid = light.transform;
        rigid.position = cy::Vec3f(0.0f); rigid.scale = cy::Vec3f(1);
        const auto direction = rigid.ToMatrix() * cy::Vec4f(0,0,-1,0);
        light.proxy.direction = cy::Vec3f(direction.x,direction.y,direction.z).GetNormalized();
    }
    renderer.UpdateLight(light.proxy.id, light.proxy);
}

