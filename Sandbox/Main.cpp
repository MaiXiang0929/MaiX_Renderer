// SPDX-License-Identifier: MIT
#include "Engine/Core/Platform/WindowContext.h"
#include "Engine/Runtime/Engine/EngineSession.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Runtime/Engine/SceneFrameBuilder.h"
#include "Engine/Asset/Geometry/CubeGeometry.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Rendering/RHI/OpenGL/OpenGLCommandList.h"
#include <glad/glad.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void VerifyComposition()
{
    auto scene = Texture2D::CreateRGBA8(1, 1, {64, 128, 192, 255}, TextureColorSpace::Linear);
    // Premultiplied red at alpha 128, as produced by EditorPrimitivePass.
    auto overlay = Texture2D::CreateRGBA8(1, 1, {128, 0, 0, 128}, TextureColorSpace::Linear);
    Require(scene && overlay, "Composition input allocation failed");
    Framebuffer target;
    FramebufferSpecification specification;
    specification.width = 8; specification.height = 8;
    specification.depthStencilEnabled = false; specification.mipmapsEnabled = false;
    Require(target.Init(specification), "Composition target allocation failed");
    const auto oldTexture = target.GetColorTexture();
    specification.width = 0;
    Require(!target.Init(specification) && target.GetColorTexture() == oldTexture,
        "Failed resize discarded the valid target");

    Shader pipeline;
    Require(pipeline.Load("Shaders/present/present.vert", "Shaders/present/present.frag"),
        "Present Shader failed to load from the new source/deployment path");
    std::vector<Vertex> vertices(6);
    const float positions[][4] = {{-1,-1,0,0}, {1,-1,1,0}, {1,1,1,1},
                                 {-1,-1,0,0}, {1,1,1,1}, {-1,1,0,1}};
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        vertices[i].Position = cy::Vec3f(positions[i][0], positions[i][1], 0);
        vertices[i].TexCoord = cy::Vec2f(positions[i][2], positions[i][3]);
    }
    Mesh geometry;
    geometry.Upload(vertices);
    target.Bind();
    glViewport(0, 0, 8, 8);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    OpenGLCommandList commands;
    const OpenGLTextureView sceneView(scene->GetID()), overlayView(overlay->GetID());
    commands.Composite(pipeline, sceneView, overlayView, geometry);
    unsigned char pixel[4]{};
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    Require(std::abs(int(pixel[0]) - 160) <= 2 && std::abs(int(pixel[1]) - 64) <= 2 &&
        std::abs(int(pixel[2]) - 96) <= 2 && pixel[3] == 255,
        "RHI composition differs from premultiplied overlay semantics");
    target.Unbind();
    Require(glGetError() == GL_NO_ERROR, "RHI composition produced an OpenGL error");
}

void VerifySession()
{
    EngineSession engine;
    Require(engine.Initialize(), "Engine initialization failed");
    Renderer* renderer = engine.GetRenderer();
    Require(engine.Initialize() && renderer == engine.GetRenderer(), "Initialize is not idempotent");
    const auto cube = CreateCubeGeometry(2.0f);
    const auto mesh = renderer->CreateMesh(cube.vertices, cube.indices);
    Material material;
    const auto parent = renderer->CreateMaterial(std::move(material));
    const auto child = renderer->CreateMaterialInstance(parent);
    Require(mesh.IsValid() && parent.IsValid() && child.IsValid(), "Shared scene resources failed");
    auto& scene = engine.Scene();
    SceneModel model;
    model.id = ModelId{scene.nextModelId++}; model.name = "Sandbox Cube";
    model.meshes.push_back(mesh); model.materials = {parent, child};
    const PrimitiveBounds bounds{cy::Vec3f(0.0f), std::sqrt(3.0f)};
    const auto primitive = renderer->AddPrimitive(mesh, child, cy::Matrix4f::Identity(), bounds);
    Require(primitive != InvalidPrimitiveId, "Scene proxy registration failed");
    SceneModelSection section;
    section.primitiveId = primitive; section.material = child; section.localBounds = bounds;
    model.sections.push_back(section); scene.models.push_back(std::move(model));
    SceneLight light;
    light.proxy.type = LightType::Directional;
    light.transform.rotationDegrees = cy::Vec3f(-35, 25, 0);
    light.proxy.id = renderer->AddLight(light.proxy);
    scene.mainLightId = light.proxy.id;
    scene.lights.push_back(light);
    ApplySceneLightTransform(scene.lights.back(), *renderer);
    SceneLight point;
    point.proxy.type = LightType::Point;
    point.transform.position = cy::Vec3f(2, 1, 0);
    point.proxy.id = renderer->AddLight(point.proxy);
    scene.lights.push_back(point);
    ApplySceneLightTransform(scene.lights.back(), *renderer);
    scene.camera.transform.position = cy::Vec3f(0, 0, 8);
    scene.modelDiameter = 2 * std::sqrt(3.0f);
    Camera navigation(cy::Vec3f(0.0f), 50);
    auto frame = BuildSceneFrame(scene, navigation, 64, 48, true, {false, 0, false});
    Require(frame.viewportWidth == 64 && frame.viewportHeight == 48 &&
        frame.cameraWorldPosition.z == 8 && !frame.editorPrimitivesEnabled,
        "Standalone frame consumed editor camera/overlay state");
    for (int i = 0; i < 3; ++i) engine.Render(frame);
    Require(renderer->GetFinalColorTexture() != 0, "Standalone engine produced no final texture");
    const auto overlayIndex = static_cast<std::size_t>(RenderPassType::EditorPrimitive);
    const auto hiddenOverlay = renderer->GetSubmissionSnapshot();
    Require(!frame.editorPrimitivesEnabled && hiddenOverlay.valid &&
        hiddenOverlay.passes[overlayIndex].drawCalls == 0,
        "Standalone rendering enabled editor light overlays");
    frame = BuildSceneFrame(scene, navigation, 64, 48, true, {false, 0, true});
    engine.Render(frame);
    Require(renderer->GetSubmissionSnapshot().passes[overlayIndex].drawCalls > 0,
        "Editor overlay validation light produced no draw");
    renderer->SetEditorPrimitivesEnabled(false);
    frame = BuildSceneFrame(scene, navigation, 64, 48, true, {false, 0, true});
    engine.Render(frame);
    Require(!frame.editorPrimitivesEnabled &&
        renderer->GetSubmissionSnapshot().passes[overlayIndex].drawCalls == 0,
        "Renderer overlay toggle did not disable editor draws");
    // All scene bindings must go before their parent material and shared mesh.
    DestroySceneModelResources(scene.models.front(), *renderer);
    Require(!renderer->DestroyMesh(mesh) && !renderer->DestroyMaterial(child) &&
        !renderer->DestroyMaterial(parent), "Bridge did not release tracked resources");
    engine.Shutdown();
    Require(!engine.GetRenderer() && engine.Scene().models.empty(), "Session did not release its state");
    engine.Shutdown();
    Require(glGetError() == GL_NO_ERROR, "Session lifecycle produced an OpenGL error");
}
}

int main()
{
    try
    {
        WindowContext context;
        std::string error;
        Require(!context.Initialize(0, 48, "Invalid", error, false) && !context.GetWindow(),
            "Invalid context creation left a partial window");
        Require(context.Initialize(64, 48, "MaiX Sandbox", error, false), error.c_str());
        VerifyComposition();
        VerifySession();
        context.Shutdown();
        Require(!context.GetWindow(), "Context shutdown did not clear its window");
        std::cout << "[Sandbox] RHI pixels, scene frame and Engine lifecycle passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Sandbox] " << error.what() << '\n';
        return 1;
    }
}
