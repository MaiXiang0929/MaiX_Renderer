cmake_minimum_required(VERSION 3.15)
project(MaiX_Renderer LANGUAGES C CXX)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build directory, such as the windows-ninja-debug preset.")
endif()
include(CTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(SRC_DIR ${PROJECT_ROOT})
set(THIRD_PARTY_DIR ${PROJECT_ROOT}/ThirdParty)
set(ASSET_DIR ${PROJECT_ROOT}/assets)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ASSET_DIR}/scenes/startup.json")
file(READ "${ASSET_DIR}/scenes/startup.json" STARTUP_SCENE_JSON)
configure_file(Engine/Runtime/Scene/StartupSceneEmbedded.h.in generated/StartupSceneEmbedded.h @ONLY)

add_library(MaiXHeaders INTERFACE)
target_include_directories(MaiXHeaders INTERFACE ${PROJECT_ROOT}
    ${THIRD_PARTY_DIR} ${THIRD_PARTY_DIR}/cyCodeBase
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(MaiXGL STATIC ThirdParty/glad/src/glad.c)
target_include_directories(MaiXGL PUBLIC ${THIRD_PARTY_DIR}/glad/include)
target_link_libraries(MaiXGL PUBLIC opengl32.lib)
add_library(MaiXPNG STATIC ThirdParty/lodepng/lodepng.cpp)
target_include_directories(MaiXPNG PUBLIC ${THIRD_PARTY_DIR}/lodepng)

add_library(MaiXCore STATIC Engine/Core/Math/Transform.cpp Engine/Core/Math/Bounds.cpp)
target_link_libraries(MaiXCore PUBLIC MaiXHeaders)

add_library(MaiXPlatform STATIC Engine/Core/Platform/Windows/FileDialog.cpp Engine/Core/Platform/WindowContext.cpp)
target_include_directories(MaiXPlatform PUBLIC ${THIRD_PARTY_DIR}/glfw/include)
target_link_directories(MaiXPlatform PUBLIC ${THIRD_PARTY_DIR}/glfw/lib-vc2022)
target_link_libraries(MaiXPlatform PUBLIC MaiXCore MaiXGL glfw3.lib PRIVATE ole32.lib shell32.lib)

add_library(MaiXAsset STATIC Engine/Asset/Importer/ImageLoader.cpp
    Engine/Asset/Importer/FbxModelImporter.cpp ThirdParty/ufbx/ufbx.c)
target_include_directories(MaiXAsset PRIVATE ${THIRD_PARTY_DIR}/stb ${THIRD_PARTY_DIR}/ufbx)
target_compile_options(MaiXAsset PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
target_link_libraries(MaiXAsset PUBLIC MaiXCore)

# Transitional CPU records keep value-only proxy/handle contracts, not Renderer implementation.
add_library(MaiXRuntime STATIC Engine/Runtime/Component/Camera.cpp
    Engine/Runtime/Scene/StartupScene.cpp Engine/Runtime/Scene/InstanceGrid.cpp
    Engine/Runtime/Scene/SceneModel.cpp Engine/Runtime/Scene/SceneLight.cpp
    Engine/Runtime/Engine/ApplicationOptions.cpp)
target_link_libraries(MaiXRuntime PUBLIC MaiXCore)

# Shared diagnostics are below both command execution and renderer orchestration.
add_library(MaiXRenderSupport STATIC
    Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.cpp
    Engine/Rendering/Renderer/Core/OpenGLStateCache.cpp)
target_link_libraries(MaiXRenderSupport PUBLIC MaiXCore MaiXGL)
add_library(MaiXRHI STATIC Engine/Rendering/RHI/OpenGL/OpenGLCommandList.cpp)
target_link_libraries(MaiXRHI PUBLIC MaiXRenderSupport)

add_library(MaiXRendering STATIC
    Engine/Rendering/Material/Material.cpp
    Engine/Rendering/Material/MaterialInstance.cpp
    Engine/Rendering/Material/MaterialResourceStore.cpp
    Engine/Rendering/Passes/BloomPass.cpp
    Engine/Rendering/Passes/EditorPrimitivePass.cpp
    Engine/Rendering/Passes/ForwardPass.cpp
    Engine/Rendering/Passes/OutlinePass.cpp
    Engine/Rendering/Passes/PostProcessPass.cpp
    Engine/Rendering/Passes/PresentPass.cpp
    Engine/Rendering/Passes/ReflectionPass.cpp
    Engine/Rendering/Passes/SSAOPass.cpp
    Engine/Rendering/Passes/ShadowPass.cpp
    Engine/Rendering/Passes/TranslucencyPass.cpp
    Engine/Rendering/Renderer/Core/Renderer.cpp
    Engine/Rendering/Renderer/Diagnostics/GpuDebugScope.cpp
    Engine/Rendering/Renderer/Diagnostics/GpuPassProfiler.cpp
    Engine/Rendering/Renderer/Pipeline/PassResourceContract.cpp
    Engine/Rendering/Renderer/Pipeline/PassResourcesGL.cpp
    Engine/Rendering/Renderer/Pipeline/RenderPipeline.cpp
    Engine/Rendering/Renderer/Resources/CubemapTexture.cpp
    Engine/Rendering/Renderer/Resources/Framebuffer.cpp
    Engine/Rendering/Renderer/Resources/InstanceBuffer.cpp
    Engine/Rendering/Renderer/Resources/Mesh.cpp
    Engine/Rendering/Renderer/Resources/Shader.cpp
    Engine/Rendering/Renderer/Resources/ShadowMap.cpp
    Engine/Rendering/Renderer/Resources/Texture2D.cpp
    Engine/Rendering/Renderer/Scene/RenderScene.cpp
    Engine/Rendering/Renderer/View/Frustum.cpp
    Engine/Rendering/Renderer/View/LightRenderData.cpp
    Engine/Rendering/Renderer/View/RenderBatch.cpp
)
target_link_libraries(MaiXRendering PUBLIC MaiXCore MaiXAsset MaiXRHI MaiXPNG)
target_include_directories(MaiXRendering PRIVATE ${THIRD_PARTY_DIR}/glfw/include)
target_link_directories(MaiXRendering PUBLIC ${THIRD_PARTY_DIR}/glfw/lib-vc2022)
target_link_libraries(MaiXRendering PUBLIC glfw3.lib)
target_compile_options(MaiXRendering PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")

add_library(MaiXEngine STATIC Engine/Runtime/Engine/EngineSession.cpp
    Engine/Runtime/Engine/RenderSceneBridge.cpp Engine/Runtime/Engine/SceneFrameBuilder.cpp)
target_link_libraries(MaiXEngine PUBLIC MaiXRuntime MaiXRendering)

add_library(MaiXEditorUI STATIC ThirdParty/imgui/imgui.cpp ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_tables.cpp ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/backends/imgui_impl_glfw.cpp ThirdParty/imgui/backends/imgui_impl_opengl3.cpp
    ThirdParty/ImGuizmo/ImGuizmo.cpp)
target_include_directories(MaiXEditorUI PUBLIC ${THIRD_PARTY_DIR}/imgui
    ${THIRD_PARTY_DIR}/ImGuizmo ${THIRD_PARTY_DIR}/glfw/include)
target_link_directories(MaiXEditorUI PUBLIC ${THIRD_PARTY_DIR}/glfw/lib-vc2022)
target_link_libraries(MaiXEditorUI PUBLIC MaiXGL glfw3.lib)

add_executable(MaiX_Renderer
    Editor/ContentBrowser/AssetImportPanel.cpp
    Editor/Diagnostics/RendererStatisticsPanel.cpp
    Editor/EditorApp/EditorApp.cpp
    Editor/EditorApp/Main.cpp
    Editor/Inspector/InspectorPanel.cpp
    Editor/MaterialEditor/MaterialEditorPanel.cpp
    Editor/SceneHierarchy/SceneHierarchyPanel.cpp
    Editor/Session/EditorFonts.cpp
    Editor/Session/EditorWorkspace.cpp
    Editor/Viewport/EditorViewportController.cpp
    Editor/Viewport/ViewportPicking.cpp
)
target_link_libraries(MaiX_Renderer PRIVATE MaiXEngine MaiXPlatform MaiXEditorUI)
target_compile_options(MaiX_Renderer PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
target_link_options(MaiX_Renderer PRIVATE $<$<CONFIG:Debug>:LINKER:/NODEFAULTLIB:MSVCRT>)

# Deploy even when only a Shader/scene/font changes and the executable is up to date.
set(MAIX_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(MaiX_Renderer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MAIX_RUNTIME_DIR}")
add_custom_target(MaiXEditorAssets ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MAIX_RUNTIME_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSET_DIR} ${MAIX_RUNTIME_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MAIX_RUNTIME_DIR}/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_ROOT}/Shaders ${MAIX_RUNTIME_DIR}/Shaders)
add_dependencies(MaiX_Renderer MaiXEditorAssets)

add_executable(MaiX_Sandbox Sandbox/Main.cpp)
target_link_libraries(MaiX_Sandbox PRIVATE MaiXEngine MaiXPlatform)
target_compile_options(MaiX_Sandbox PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
target_link_options(MaiX_Sandbox PRIVATE $<$<CONFIG:Debug>:LINKER:/NODEFAULTLIB:MSVCRT>)
add_dependencies(MaiX_Sandbox MaiXEditorAssets)

if(BUILD_TESTING)
    add_test(NAME EngineRuntimeSmoke COMMAND MaiX_Sandbox)
    set_tests_properties(EngineRuntimeSmoke PROPERTIES WORKING_DIRECTORY "${PROJECT_ROOT}")
    add_executable(EditorFontsTests tests/Editor/EditorFontsTests.cpp Editor/Session/EditorFonts.cpp
        ThirdParty/imgui/imgui.cpp ThirdParty/imgui/imgui_draw.cpp
        ThirdParty/imgui/imgui_tables.cpp ThirdParty/imgui/imgui_widgets.cpp)
    target_include_directories(EditorFontsTests PRIVATE ${SRC_DIR} ${THIRD_PARTY_DIR}/imgui)
    add_test(NAME ModuleBoundaries COMMAND ${CMAKE_COMMAND} -DPROJECT_ROOT=${PROJECT_ROOT} -P ${PROJECT_ROOT}/tests/Architecture/CheckModuleBoundaries.cmake)
    add_test(NAME EditorFontsTests COMMAND EditorFontsTests "${ASSET_DIR}/fonts/NotoSansSC.ttf")

    add_executable(StartupSceneTests tests/Core/StartupSceneTests.cpp
        Engine/Runtime/Scene/StartupScene.cpp Engine/Core/Math/Transform.cpp Engine/Runtime/Component/Camera.cpp)
    target_include_directories(StartupSceneTests PRIVATE ${SRC_DIR} ${THIRD_PARTY_DIR} "${CMAKE_CURRENT_BINARY_DIR}/generated"
        ${THIRD_PARTY_DIR}/cyCodeBase ${THIRD_PARTY_DIR}/glad/include)
    add_test(NAME StartupSceneTests COMMAND StartupSceneTests)

    add_executable(
        FrustumTests
        tests/Renderer/View/FrustumTests.cpp
        Engine/Rendering/Renderer/View/Frustum.cpp
        Engine/Core/Math/Bounds.cpp
    )

    target_include_directories(
        FrustumTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME FrustumTests COMMAND FrustumTests)

    add_executable(
        RenderSceneTests
        tests/Renderer/Scene/RenderSceneTests.cpp
        Engine/Rendering/Renderer/Scene/RenderScene.cpp
        Engine/Core/Math/Bounds.cpp
        Engine/Rendering/Renderer/View/Frustum.cpp
        Engine/Rendering/Renderer/View/RenderBatch.cpp
    )

    target_include_directories(
        RenderSceneTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME RenderSceneTests COMMAND RenderSceneTests)

    add_executable(
        LightRenderDataTests
        tests/Renderer/View/LightRenderDataTests.cpp
        Engine/Rendering/Renderer/View/LightRenderData.cpp
    )

    target_include_directories(
        LightRenderDataTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME LightRenderDataTests COMMAND LightRenderDataTests)

    add_executable(
        RenderTargetSizingTests
        tests/Renderer/Pipeline/RenderTargetSizingTests.cpp
    )

    target_include_directories(
        RenderTargetSizingTests
        PRIVATE
        ${SRC_DIR}
    )

    add_test(NAME RenderTargetSizingTests COMMAND RenderTargetSizingTests)

    add_executable(
        FramebufferTests
        tests/Renderer/Resources/FramebufferTests.cpp
        Engine/Rendering/Renderer/Resources/Framebuffer.cpp
        Engine/Rendering/Renderer/Pipeline/PassResourceContract.cpp
        Engine/Rendering/Renderer/Pipeline/PassResourcesGL.cpp
        ThirdParty/glad/src/glad.c
    )

    target_include_directories(
        FramebufferTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/glad/include
        ${THIRD_PARTY_DIR}/glfw/include
    )

    target_link_directories(FramebufferTests PRIVATE
        ${THIRD_PARTY_DIR}/glfw/lib-vc2022)
    target_link_libraries(FramebufferTests PRIVATE glfw3.lib opengl32.lib)
    target_compile_options(FramebufferTests PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
    target_link_options(FramebufferTests PRIVATE
        $<$<CONFIG:Debug>:LINKER:/NODEFAULTLIB:MSVCRT>)
    add_test(NAME FramebufferTests COMMAND FramebufferTests)

    add_executable(
        PostProcessSettingsTests
        tests/Renderer/Pipeline/PostProcessSettingsTests.cpp
    )

    target_include_directories(
        PostProcessSettingsTests
        PRIVATE
        ${SRC_DIR}
    )

    add_test(NAME PostProcessSettingsTests COMMAND PostProcessSettingsTests)

    add_executable(
        ApplicationBenchmarkTests
        tests/Core/ApplicationBenchmarkTests.cpp
        Engine/Runtime/Engine/ApplicationOptions.cpp
        Engine/Runtime/Scene/InstanceGrid.cpp
    )

    target_include_directories(
        ApplicationBenchmarkTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME ApplicationBenchmarkTests COMMAND ApplicationBenchmarkTests)

    add_executable(
        RenderBatchTests
        tests/Renderer/View/RenderBatchTests.cpp
        Engine/Rendering/Renderer/View/RenderBatch.cpp
    )

    target_include_directories(
        RenderBatchTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME RenderBatchTests COMMAND RenderBatchTests)

    add_executable(
        MaterialTypesTests
        tests/Renderer/Resources/MaterialTypesTests.cpp
    )

    target_include_directories(
        MaterialTypesTests
        PRIVATE
        ${SRC_DIR}
    )

    add_test(NAME MaterialTypesTests COMMAND MaterialTypesTests)

    # 使用真正的 Renderer 与资源表验证实例依赖和替换，不复制生产实现。
    set(MATERIAL_INSTANCE_TEST_SOURCES)
    add_executable(MaterialInstanceTests
        tests/Renderer/Resources/MaterialInstanceTests.cpp
        ${MATERIAL_INSTANCE_TEST_SOURCES}
        )
    target_include_directories(MaterialInstanceTests PRIVATE
        ${SRC_DIR} ${THIRD_PARTY_DIR}/cyCodeBase
        ${THIRD_PARTY_DIR}/glad/include ${THIRD_PARTY_DIR}/glfw/include
        ${THIRD_PARTY_DIR}/lodepng)
    target_link_directories(MaterialInstanceTests PRIVATE ${THIRD_PARTY_DIR}/glfw/lib-vc2022)
    target_link_libraries(MaterialInstanceTests PRIVATE MaiXRendering glfw3.lib opengl32.lib)
    target_compile_options(MaterialInstanceTests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>")
    target_link_options(MaterialInstanceTests PRIVATE $<$<CONFIG:Debug>:LINKER:/NODEFAULTLIB:MSVCRT>)
    add_test(NAME MaterialInstanceTests COMMAND MaterialInstanceTests)

    add_executable(
        RenderPassContractTests
        tests/Renderer/Pipeline/RenderPassContractTests.cpp
        Engine/Rendering/Renderer/Pipeline/PassResourceContract.cpp
    )

    target_include_directories(
        RenderPassContractTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/glad/include
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME RenderPassContractTests COMMAND RenderPassContractTests)

    add_executable(
        ImageLoaderTests
        tests/Assets/Import/ImageLoaderTests.cpp
        Engine/Asset/Importer/ImageLoader.cpp
    )

    target_include_directories(
        ImageLoaderTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/stb
    )

    target_compile_options(ImageLoaderTests PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>"
    )

    add_test(NAME ImageLoaderTests COMMAND ImageLoaderTests)

    add_executable(
        FbxModelImporterTests
        tests/Assets/Import/FbxModelImporterTests.cpp
        Engine/Asset/Importer/FbxModelImporter.cpp
        Engine/Asset/Importer/ImageLoader.cpp
        ThirdParty/ufbx/ufbx.c
    )

    target_include_directories(
        FbxModelImporterTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
        ${THIRD_PARTY_DIR}/glad/include
        ${THIRD_PARTY_DIR}/stb
        ${THIRD_PARTY_DIR}/ufbx
    )

    target_compile_options(FbxModelImporterTests PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>"
    )

    add_test(NAME FbxModelImporterTests COMMAND FbxModelImporterTests)

    add_executable(
        CameraTests
        tests/Core/CameraTests.cpp
        Engine/Runtime/Component/Camera.cpp
    )

    target_include_directories(
        CameraTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME CameraTests COMMAND CameraTests)

    add_executable(
        EditorInteractionTests
        tests/Editor/EditorInteractionTests.cpp
        Engine/Runtime/Component/Camera.cpp
        Engine/Core/Math/Transform.cpp
        Engine/Runtime/Scene/SceneModel.cpp
        Editor/Viewport/ViewportPicking.cpp
        Engine/Core/Math/Bounds.cpp
    )

    target_include_directories(
        EditorInteractionTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME EditorInteractionTests COMMAND EditorInteractionTests)
endif()
