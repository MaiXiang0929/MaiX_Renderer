// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Renderer/Core/Renderer.h"
#include "Renderer/View/RenderView.h"
#include "Renderer/View/RenderBatch.h"

namespace
{
void Require(bool ok, const char* message)
{
    if (!ok) { std::cerr << "[MaterialInstanceTests] " << message << std::endl; std::exit(1); }
}
bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }

void TestInheritance()
{
    MaterialResourceStore store;
    Material base;
    base.SetName("Parent");
    base.GetProperties().roughness = 0.2f;
    const auto parent = store.Create(base);
    const auto a = store.CreateInstance(parent, "A");
    const auto b = store.CreateInstance(parent, "B");
    Require(a.IsValid() && b.IsValid(), "Instances must be created.");
    Require(!store.CreateInstance(a).IsValid() && !store.CreateInstance({}).IsValid(),
        "Nested and invalid parents must be rejected.");
    const Material* address = &store.Get(a)->effective;
    auto values = store.Get(a)->effective.GetProperties();
    values.baseColor = cy::Vec3f(0.8f, 0.1f, 0.2f);
    Require(store.UpdateOverride(a, MaterialParameter::BaseColor, values), "Color override failed.");
    auto parentValues = base.GetProperties();
    parentValues.baseColor = cy::Vec3f(0.3f, 0.5f, 0.7f);
    parentValues.roughness = 0.7f;
    parentValues.shadingModel = ShadingModel::Toon;
    parentValues.outlineEnabled = true;
    Require(store.UpdateBase(parent, parentValues, BlendMode::AlphaBlend), "Parent update failed.");
    Require(Near(store.Get(a)->effective.GetProperties().baseColor.x, 0.8f) &&
            Near(store.Get(b)->effective.GetProperties().baseColor.x, 0.3f),
        "Parent changes must preserve only explicit overrides.");
    Require(Near(store.Get(a)->effective.GetProperties().roughness, 0.7f) &&
            store.Get(a)->effective.GetBlendMode() == BlendMode::AlphaBlend &&
            store.Get(a)->effective.GetProperties().shadingModel == ShadingModel::Toon &&
            store.Get(a)->effective.GetProperties().outlineEnabled,
        "Unmodified parameters and parent policy must propagate.");
    values = store.Get(a)->effective.GetProperties();
    values.roughness = -1.0f;
    values.baseColor = cy::Vec3f(0.0f); // 不属于本次更新的字段不能污染已有覆盖。
    Require(store.UpdateOverride(a, MaterialParameter::Roughness, values), "Roughness override failed.");
    Require(Near(store.Get(a)->effective.GetProperties().roughness, 0.045f) &&
            Near(store.Get(a)->effective.GetProperties().baseColor.x, 0.8f),
        "Single-field writes and normalization must be respected.");
    Require(!store.UpdateBase(a, values, BlendMode::Opaque) &&
            !store.UpdateOverride(a, MaterialParameter::Count, values),
        "Instances cannot be edited as base materials or use invalid parameters.");
    Require(store.UpdateOverride(a, MaterialParameter::BaseColor, values, false) &&
            Near(store.Get(a)->effective.GetProperties().baseColor.x, 0.3f),
        "Reset must inherit the CURRENT parent value.");
    values.faceForwardLocal = cy::Vec3f(0.0f);
    values.faceRightLocal = cy::Vec3f(0.0f);
    store.UpdateOverride(a, MaterialParameter::FaceFrame, values);
    const auto& frame = store.Get(a)->effective.GetProperties();
    Require(Near(frame.faceForwardLocal.Length(), 1.0f) &&
            Near(frame.faceRightLocal.Length(), 1.0f) &&
            Near(frame.faceForwardLocal.Dot(frame.faceRightLocal), 0.0f),
        "Face basis override must remain orthonormal.");
    Require(store.ResetOverrides(a) && store.Get(a)->instance->overrides.none(),
        "Reset all overrides failed.");
    for (int i = 0; i < 64; ++i) store.Create(Material{});
    Require(&store.Get(a)->effective == address, "Effective material address must remain stable.");
    Require(!store.Destroy(parent), "Even unbound instances must protect their parent.");
    Require(store.Destroy(a) && store.Destroy(b) && store.Destroy(parent),
        "Child-before-parent destruction must succeed.");
    Require(!store.Get(a) && !store.Destroy(a), "Destroyed handle must stay invalid.");
}

void TestSceneReplacement()
{
    MaterialResourceStore store;
    const auto parent = store.Create(Material{});
    const auto a = store.CreateInstance(parent);
    const auto b = store.CreateInstance(parent);
    RenderScene scene;
    PrimitiveSceneProxy proxy;
    proxy.mesh = reinterpret_cast<Mesh*>(static_cast<std::uintptr_t>(1));
    proxy.material = &store.Get(parent)->effective;
    proxy.materialId = parent.id;
    proxy.meshId = 5;
    const auto p = scene.AddPrimitive(proxy);
    const auto q = scene.AddPrimitive(proxy);
    Require(!scene.ReplacePrimitiveMaterials({p, InvalidPrimitiveId}, parent.id, a.id,
        &store.Get(a)->effective, BlendMode::AlphaBlend) && scene.HasMaterialReference(parent.id),
        "A failed replacement must retain all original bindings.");
    Require(scene.ReplacePrimitiveMaterials({p}, parent.id, a.id,
        &store.Get(a)->effective, BlendMode::AlphaBlend), "Valid replacement failed.");
    Require(!scene.ReplacePrimitiveMaterials({p, q}, parent.id, b.id,
        &store.Get(b)->effective, BlendMode::Opaque), "Stale expected material must reject entire batch.");
    RenderView view;
    scene.BuildRenderView(view);
    Require(view.translucentItems.size() == 1 && view.opaqueItems.size() == 1 &&
            view.translucentItems[0].materialId == a.id &&
            view.translucentItems[0].material == &store.Get(a)->effective,
        "Replacement must change queue, pointer, and material identity together.");
    scene.UpdateMaterialBlendMode(a.id, BlendMode::Opaque);
    scene.BuildRenderView(view);
    Require(view.opaqueItems.size() == 2 && view.opaqueBatches.size() == 2,
        "Different material identities cannot instance together.");
    Require(scene.ReplacePrimitiveMaterials({q}, parent.id, a.id,
        &store.Get(a)->effective, BlendMode::Opaque), "Shared instance assignment failed.");
    scene.BuildRenderView(view);
    Require(view.opaqueBatches.size() == 1 && view.opaqueBatches[0].itemCount == 2,
        "One shared instance should preserve existing batching.");
    scene.RemovePrimitive(p);
    scene.RemovePrimitive(q);
    Require(!scene.HasMaterialReference(a.id), "Removing primitives must release instance references.");
}

void TestGpuResources()
{
    Renderer renderer; // 不初始化整个管线；测试真实资源门面及 Scene 引用保护。
    Material base;
    base.SetName("Parent");
    const auto parent = renderer.CreateMaterial(base);
    const auto a = renderer.CreateMaterialInstance(parent);
    const auto b = renderer.CreateMaterialInstance(parent);
    const std::vector<Vertex> triangle = {
        {{0,0,0},{0,0,1},{0,0}}, {{1,0,0},{0,0,1},{1,0}}, {{0,1,0},{0,0,1},{0,1}}};
    const auto mesh = renderer.CreateMesh(triangle);
    const auto p = renderer.AddPrimitive(mesh, a, cy::Matrix4f::Identity());
    const auto q = renderer.AddPrimitive(mesh, a, cy::Matrix4f::Identity());
    Require(p != InvalidPrimitiveId && q != InvalidPrimitiveId, "Actual mesh submission failed.");
    Require(!renderer.DestroyMaterial(parent) && !renderer.DestroyMaterial(a),
        "Renderer must protect both parent dependency and primitive reference.");
    Require(!renderer.ReplacePrimitiveMaterials({p, InvalidPrimitiveId}, a, b) &&
            !renderer.DestroyMaterial(a), "Failed facade replacement must preserve references.");
    renderer.RemovePrimitive(p);
    Require(!renderer.DestroyMaterial(a), "Last shared primitive must keep instance alive.");
    // 零尺寸帧仍建立 RenderView，但不提交管线 GPU 工作，直接验证真实门面的分类同步。
    RenderFrameData frame;
    frame.view = frame.projection = frame.lightVP = frame.reflectionView = cy::Matrix4f::Identity();
    Renderer::MaterialSnapshot policy;
    renderer.GetMaterialSnapshot(parent, policy);
    Require(renderer.UpdateMaterial(parent, policy.properties, BlendMode::AlphaBlend), "Parent policy update failed.");
    renderer.ExecutePipeline(frame);
    Require(renderer.GetStatisticsSnapshot().translucentDrawCount == 1 &&
            renderer.GetStatisticsSnapshot().opaqueDrawCount == 0,
        "Parent blend policy must move child primitives into translucency queue.");
    renderer.UpdateMaterial(parent, policy.properties, BlendMode::Opaque);
    renderer.ExecutePipeline(frame);
    Require(renderer.GetStatisticsSnapshot().opaqueDrawCount == 1 &&
            renderer.GetStatisticsSnapshot().translucentDrawCount == 0,
        "Reset parent blend policy must restore the opaque queue.");
    Require(renderer.ReplacePrimitiveMaterials({q}, a, b) && renderer.DestroyMaterial(a),
        "Replacing last primitive must release old instance.");
    auto srgb = Texture2D::CreateRGBA8(1, 1, {180, 90, 40, 255}, TextureColorSpace::SRGB);
    auto linear = Texture2D::CreateRGBA8(1, 1, {128, 128, 255, 255}, TextureColorSpace::Linear);
    Require(srgb && linear, "Actual texture creation failed.");
    Require(renderer.UpdateMaterialTexture(parent, MaterialTextureSlot::BaseColor, srgb, "ParentTexture"),
        "Parent texture update failed.");
    Renderer::MaterialSnapshot snapshot;
    renderer.GetMaterialSnapshot(b, snapshot);
    Require(snapshot.textureIds[0] == srgb->GetID() &&
            snapshot.textureModes[0] == TextureOverrideMode::Inherit, "Inherited texture must share GPU ID.");
    Require(!renderer.UpdateMaterialTexture(b, MaterialTextureSlot::BaseColor, linear, "WrongSpace"),
        "Color-space mismatch must be rejected.");
    Require(renderer.ClearMaterialTexture(b, MaterialTextureSlot::BaseColor), "Disable failed.");
    renderer.GetMaterialSnapshot(b, snapshot);
    Require(!snapshot.hasTextures[0] && snapshot.textureModes[0] == TextureOverrideMode::Disabled,
        "Clear means explicit disable.");
    Require(renderer.ResetMaterialTexture(b, MaterialTextureSlot::BaseColor), "Reset texture failed.");
    renderer.GetMaterialSnapshot(b, snapshot);
    Require(snapshot.textureIds[0] == srgb->GetID(), "Texture reset must restore parent.");
    auto replacement = Texture2D::CreateRGBA8(1, 1, {20, 180, 30, 255}, TextureColorSpace::SRGB);
    Require(renderer.UpdateMaterialTexture(b, MaterialTextureSlot::BaseColor, replacement, "Replacement"),
        "Replacement failed.");
    renderer.ClearMaterialTexture(parent, MaterialTextureSlot::BaseColor);
    renderer.GetMaterialSnapshot(b, snapshot);
    Require(snapshot.textureIds[0] == replacement->GetID() &&
            snapshot.textureModes[0] == TextureOverrideMode::Replace,
        "Parent changes must not erase a replaced texture.");
    renderer.ResetMaterialOverrides(b);
    renderer.GetMaterialSnapshot(b, snapshot);
    Require(!snapshot.hasTextures[0] && snapshot.textureModes[0] == TextureOverrideMode::Inherit,
        "Reset all must clear texture overrides.");
    // 所有资源必须在上下文销毁前释放，释放最后共享引用后纹理对象也应被回收。
    const GLuint replacedId = replacement->GetID();
    replacement.reset();
    Require(glIsTexture(replacedId) == GL_FALSE, "Unused replacement GPU texture should be released.");
    renderer.RemovePrimitive(q);
    Require(renderer.DestroyMaterial(b) && renderer.DestroyMaterial(parent) && renderer.DestroyMesh(mesh),
        "Actual child-before-parent cleanup failed.");
    Require(renderer.GetMaterialResourceCount() == 0 && renderer.GetMeshResourceCount() == 0,
        "Resource counts should return to zero.");
    Require(glGetError() == GL_NO_ERROR, "Resource tests produced OpenGL errors.");
}
}
int main()
{
    TestInheritance();
    TestSceneReplacement();
    if (!glfwInit())
    {
        std::cout << "MaterialInstanceTests CPU passed; GPU checks skipped (GLFW unavailable)." << std::endl;
        return 0;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(32, 32, "MaterialInstanceTests", nullptr, nullptr);
    if (!window) { glfwTerminate(); std::cout << "MaterialInstanceTests CPU passed; GPU checks skipped (context unavailable)." << std::endl; return 0; }
    glfwMakeContextCurrent(window);
    Require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GL loader failed.");
    TestGpuResources();
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "MaterialInstanceTests CPU and GPU passed." << std::endl;
    return 0;
}
