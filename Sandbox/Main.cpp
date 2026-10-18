// SPDX-License-Identifier: MIT
#include "Engine/Core/Platform/WindowContext.h"
#include "Engine/Runtime/Engine/EngineSession.h"
#include "Engine/Runtime/Engine/RenderSceneBridge.h"
#include "Engine/Runtime/Engine/SceneFrameBuilder.h"
#include "Engine/Asset/Geometry/CubeGeometry.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Rendering/RHI/OpenGL/OpenGLNativeBridge.h"
#include <glad/glad.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <chrono>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

PFNGLCREATESHADERPROC realCreateShader = nullptr;
std::vector<GLuint> createdShaders;
GLuint APIENTRY TrackShaderCreation(GLenum stage)
{
    const GLuint shader = realCreateShader(stage);
    createdShaders.push_back(shader);
    return shader;
}

PFNGLDRAWARRAYSPROC realDrawArrays = nullptr;
bool injectedDrawFailure = false;
void APIENTRY FailOneDraw(GLenum mode, GLint first, GLsizei count)
{
    realDrawArrays(mode, first, count);
    if (!injectedDrawFailure)
    {
        injectedDrawFailure = true;
        glEnable(0xffffffff); // Real backend error, consumed by RHI Draw.
    }
}

void VerifyShaderFailureCleanup()
{
    auto device=CreateRHIDevice();
    Shader shader(*device);
    Require(shader.Load("Shaders/present/present.vert", "Shaders/present/present.frag"),
        "Shader reload baseline failed");
    const auto oldProgram = shader.GetProgramID();
    const auto path = std::filesystem::temp_directory_path() /
        ("maix-invalid-shader-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()) + ".glsl");
    std::ofstream(path) << "#version 400 core\nintentional compile failure\n";
    struct Restore
    {
        std::filesystem::path path;
        ~Restore() { glad_glCreateShader = realCreateShader; std::filesystem::remove(path); }
    } restore{path};
    realCreateShader = glad_glCreateShader;
    glad_glCreateShader = TrackShaderCreation;
    for (bool invalidVertex : {false, true})
    {
        createdShaders.clear();
        Require(!shader.Load(invalidVertex ? path.string() : "Shaders/present/present.vert",
            invalidVertex ? "Shaders/present/present.frag" : path.string()),
            "Invalid shader unexpectedly compiled");
        Require(shader.GetProgramID() == oldProgram && glIsProgram(GetOpenGLProgram(*device,oldProgram)),
            "Failed reload discarded the working program");
        for (GLuint object : createdShaders)
            Require(!glIsShader(object), "Failed reload leaked a compiled shader stage");
    }
    // Each stage compiles, but its interface disagrees with present.vert's vec2 UV.
    std::ofstream(path) << "#version 330 core\nin vec3 fragTexCoord;\nout vec4 color;\n"
        "void main(){ color=vec4(fragTexCoord,1); }\n";
    createdShaders.clear();
    Require(!shader.Load("Shaders/present/present.vert", path.string()),
        "Mismatched shader interface unexpectedly linked");
    Require(shader.GetProgramID() == oldProgram && glIsProgram(GetOpenGLProgram(*device, oldProgram)),
        "Link failure discarded the working program");
    for (GLuint object : createdShaders)
        Require(!glIsShader(object), "Link failure leaked a compiled shader stage");
}

void VerifyComposition()
{
    auto device=CreateRHIDevice();
    auto scene = Texture2D::CreateRGBA8(*device,1, 1, {64, 128, 192, 255}, TextureColorSpace::Linear);
    // Premultiplied red at alpha 128, as produced by EditorPrimitivePass.
    auto overlay = Texture2D::CreateRGBA8(*device,1, 1, {128, 0, 0, 128}, TextureColorSpace::Linear);
    Require(scene && overlay, "Composition input allocation failed");
    Framebuffer target(*device);
    FramebufferSpecification specification;
    specification.width = 8; specification.height = 8;
    specification.depthStencilEnabled = false; specification.mipmapsEnabled = false;
    Require(target.Init(specification), "Composition target allocation failed");
    const auto oldTexture = target.GetColorTexture();
    specification.width = 0;
    Require(!target.Init(specification) && target.GetColorTexture() == oldTexture,
        "Failed resize discarded the valid target");

    Shader pipeline(*device);
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
    Mesh geometry(*device);
    geometry.Upload(vertices);
    target.Bind();
    glViewport(0, 0, 8, 8);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    pipeline.Bind();
    device->Commands().BindTexture(scene->GetID(),0); device->Commands().BindTexture(overlay->GetID(),1);
    pipeline.SetInt("renderedTexture",0); pipeline.SetInt("editorOverlayTexture",1);
    geometry.Draw();
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
    realDrawArrays = glad_glDrawArrays;
    glad_glDrawArrays = FailOneDraw;
    struct RestoreDraw
    {
        ~RestoreDraw() { glad_glDrawArrays = realDrawArrays; }
    } restoreDraw;
    engine.Render(frame);
    glad_glDrawArrays = realDrawArrays;
    Require(injectedDrawFailure && renderer->GetFinalColorTexture() == 0,
        "Failed frame exposed a final texture");
    for (int i = 0; i < 4; ++i) engine.Render(frame);
    Require(renderer->GetFinalColorTexture() != 0,
        "Rendering did not recover after a failed frame");
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
        VerifyShaderFailureCleanup();
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
