cmake_minimum_required(VERSION 3.15)

project(MaiX_Renderer)

include(CTest)

# ============================
# C++ 标准
# ============================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# MSVC 使用 UTF-8 编码
add_compile_options(
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

# ============================
# 目录定义
# ============================
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(SRC_DIR
    ${PROJECT_ROOT}
)

set(THIRD_PARTY_DIR
    ${PROJECT_ROOT}/ThirdParty
)

set(ASSET_DIR
    ${PROJECT_ROOT}/assets
)

# ============================
# Source Files
# ============================

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ASSET_DIR}/scenes/startup.json")
file(READ "${ASSET_DIR}/scenes/startup.json" STARTUP_SCENE_JSON)
configure_file(Engine/Runtime/Scene/StartupSceneEmbedded.h.in generated/StartupSceneEmbedded.h @ONLY)

set(SOURCES

    # 程序入口
    Editor/EditorApp/Main.cpp

    # Core
    Editor/EditorApp/Application.cpp
    Engine/Runtime/Engine/ApplicationOptions.cpp
    Engine/Runtime/Scene/StartupScene.cpp
    Engine/Runtime/Component/Camera.cpp
    Engine/Runtime/Scene/InstanceGrid.cpp
    Engine/Core/Math/Transform.cpp

    # Editor
    Editor/Diagnostics/RendererStatisticsPanel.cpp
    Editor/MaterialEditor/MaterialEditorPanel.cpp
    Editor/ContentBrowser/AssetImportPanel.cpp
    Editor/Session/EditableLight.cpp
    Editor/Session/EditableModel.cpp
    Editor/Session/EditableModelRenderer.cpp
    Editor/Viewport/EditorViewportController.cpp
    Editor/Session/EditorWorkspace.cpp
    Editor/Session/EditorFonts.cpp
    Editor/SceneHierarchy/SceneHierarchyPanel.cpp
    Editor/Inspector/InspectorPanel.cpp
    Editor/Viewport/ViewportPicking.cpp

    # Platform
    Engine/Core/Platform/Windows/FileDialog.cpp

    # Asset Import
    Engine/Asset/Importer/ImageLoader.cpp
    Engine/Asset/Importer/FbxModelImporter.cpp

    # Renderer Core
    Engine/Rendering/Renderer/Core/Renderer.cpp
    Engine/Rendering/Renderer/Core/OpenGLStateCache.cpp

    # Renderer Diagnostics
    Engine/Rendering/Renderer/Diagnostics/GpuDebugScope.cpp
    Engine/Rendering/Renderer/Diagnostics/GpuPassProfiler.cpp
    Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.cpp

    # Renderer Pipeline
    Engine/Rendering/Renderer/Pipeline/RenderPipeline.cpp
    Engine/Rendering/Renderer/Pipeline/PassResourceContract.cpp
    Engine/Rendering/Renderer/Pipeline/PassResourcesGL.cpp

    # Renderer Scene
    Engine/Rendering/Renderer/Scene/RenderScene.cpp
    Engine/Rendering/Renderer/Scene/PrimitiveSceneProxy.cpp

    # Renderer View
    Engine/Rendering/Renderer/View/Frustum.cpp
    Engine/Rendering/Renderer/View/LightRenderData.cpp
    Engine/Rendering/Renderer/View/RenderBatch.cpp

    # Renderer Passes
    Engine/Rendering/Passes/ForwardPass.cpp
    Engine/Rendering/Passes/OutlinePass.cpp
    Engine/Rendering/Passes/SSAOPass.cpp
    Engine/Rendering/Passes/TranslucencyPass.cpp
    Engine/Rendering/Passes/EditorPrimitivePass.cpp
    Engine/Rendering/Passes/ShadowPass.cpp
    Engine/Rendering/Passes/ReflectionPass.cpp
    Engine/Rendering/Passes/BloomPass.cpp
    Engine/Rendering/Passes/PostProcessPass.cpp
    Engine/Rendering/Passes/PresentPass.cpp

    # Renderer Resources
    Engine/Rendering/Renderer/Resources/Shader.cpp
    Engine/Rendering/Renderer/Resources/Mesh.cpp
    Engine/Rendering/Renderer/Resources/Texture2D.cpp
    Engine/Rendering/Material/Material.cpp
    Engine/Rendering/Material/MaterialInstance.cpp
    Engine/Rendering/Material/MaterialResourceStore.cpp
    Engine/Rendering/Renderer/Resources/CubemapTexture.cpp
    Engine/Rendering/Renderer/Resources/Framebuffer.cpp
    Engine/Rendering/Renderer/Resources/InstanceBuffer.cpp
    Engine/Rendering/Renderer/Resources/ShadowMap.cpp

    # GLAD
    ThirdParty/glad/src/glad.c

    # LodePNG
    ThirdParty/lodepng/lodepng.cpp

    # ufbx
    ThirdParty/ufbx/ufbx.c

    # Dear ImGui v1.91.9b-docking（含 GLFW/OpenGL 后端，支持编辑器停靠布局）
    ThirdParty/imgui/imgui.cpp
    ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_tables.cpp
    ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/backends/imgui_impl_glfw.cpp
    ThirdParty/imgui/backends/imgui_impl_opengl3.cpp

    # ImGuizmo (pinned to 18cef5e031d8c6973d80284c67f60549fafd78c1)
    ThirdParty/ImGuizmo/ImGuizmo.cpp
)

# ============================
# 创建可执行文件
# ============================
add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
 )

target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>"
)

# ============================
# Include Path
# ============================
target_include_directories(MaiX_Renderer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" "${THIRD_PARTY_DIR}")

target_include_directories(
    ${PROJECT_NAME}

    PRIVATE

    # src
    ${SRC_DIR}

    # Core
    ${PROJECT_ROOT}/Engine/Core

    # Editor
    ${PROJECT_ROOT}/Editor

    # GLAD
    ${THIRD_PARTY_DIR}/glad/include

    # GLFW
    ${THIRD_PARTY_DIR}/glfw/include

    # cyCodeBase
    ${THIRD_PARTY_DIR}/cyCodeBase

    # LodePNG
    ${THIRD_PARTY_DIR}/lodepng

    # Dear ImGui
    ${THIRD_PARTY_DIR}/imgui

    # ImGuizmo
    ${THIRD_PARTY_DIR}/ImGuizmo

    # stb_image
    ${THIRD_PARTY_DIR}/stb

    # ufbx
    ${THIRD_PARTY_DIR}/ufbx
)

# ============================
# GLFW
# ============================
target_link_directories(
    ${PROJECT_NAME}

    PRIVATE

    ${THIRD_PARTY_DIR}/glfw/lib-vc2022
)

target_link_libraries(
    ${PROJECT_NAME}

    PRIVATE

    glfw3.lib

    opengl32.lib

    ole32.lib

    shell32.lib
)

# Debug 配置下排除 Release CRT，解决与预编译 glfw3.lib 的 MSVCRT/MSVCRTD 混用冲突
# 该冲突会导致 Run-Time Check Failure #2（栈损坏）
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:LINKER:/NODEFAULTLIB:MSVCRT>
)

# ============================
# Copy Assets
# ============================
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E remove_directory
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets

    COMMAND ${CMAKE_COMMAND} -E copy_directory

    ${ASSET_DIR}

    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets

    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_ROOT}/Shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders
)

# ============================
# Visual Studio 文件结构
# ============================
source_group(
    TREE ${PROJECT_ROOT}

    FILES

    ${SOURCES}
)

if(BUILD_TESTING)
    add_executable(EditorFontsTests tests/Editor/EditorFontsTests.cpp Editor/Session/EditorFonts.cpp
        ThirdParty/imgui/imgui.cpp ThirdParty/imgui/imgui_draw.cpp
        ThirdParty/imgui/imgui_tables.cpp ThirdParty/imgui/imgui_widgets.cpp)
    target_include_directories(EditorFontsTests PRIVATE ${SRC_DIR} ${THIRD_PARTY_DIR}/imgui)
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
        Engine/Rendering/Renderer/Scene/PrimitiveSceneProxy.cpp
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
        Engine/Rendering/Renderer/Scene/PrimitiveSceneProxy.cpp
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
    set(MATERIAL_INSTANCE_TEST_SOURCES ${SOURCES})
    list(FILTER MATERIAL_INSTANCE_TEST_SOURCES INCLUDE REGEX "^Engine/Rendering/")
    add_executable(MaterialInstanceTests
        tests/Renderer/Resources/MaterialInstanceTests.cpp
        ${MATERIAL_INSTANCE_TEST_SOURCES}
        ThirdParty/glad/src/glad.c
        ThirdParty/lodepng/lodepng.cpp)
    target_include_directories(MaterialInstanceTests PRIVATE
        ${SRC_DIR} ${THIRD_PARTY_DIR}/cyCodeBase
        ${THIRD_PARTY_DIR}/glad/include ${THIRD_PARTY_DIR}/glfw/include
        ${THIRD_PARTY_DIR}/lodepng)
    target_link_directories(MaterialInstanceTests PRIVATE ${THIRD_PARTY_DIR}/glfw/lib-vc2022)
    target_link_libraries(MaterialInstanceTests PRIVATE glfw3.lib opengl32.lib)
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
        Editor/Session/EditableModel.cpp
        Editor/Viewport/ViewportPicking.cpp
        Engine/Rendering/Renderer/Scene/PrimitiveSceneProxy.cpp
    )

    target_include_directories(
        EditorInteractionTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME EditorInteractionTests COMMAND EditorInteractionTests)
endif()
