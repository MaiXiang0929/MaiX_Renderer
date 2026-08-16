cmake_minimum_required(VERSION 3.15)

project(OpenGL_Project)

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
    ${PROJECT_ROOT}/src
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

set(SOURCES

    # 程序入口
    src/main.cpp

    # Core
    src/Core/Application.cpp
    src/Core/ApplicationOptions.cpp
    src/Core/Camera.cpp
    src/Core/InstanceGrid.cpp

    # Editor
    src/Editor/RendererStatisticsPanel.cpp
    src/Editor/MaterialEditorPanel.cpp

    # Renderer Core
    src/Renderer/Core/Renderer.cpp
    src/Renderer/Core/OpenGLStateCache.cpp

    # Renderer Diagnostics
    src/Renderer/Diagnostics/GpuDebugScope.cpp
    src/Renderer/Diagnostics/GpuPassProfiler.cpp
    src/Renderer/Diagnostics/RenderSubmissionStats.cpp

    # Renderer Pipeline
    src/Renderer/Pipeline/RenderPipeline.cpp

    # Renderer Scene
    src/Renderer/Scene/RenderScene.cpp
    src/Renderer/Scene/PrimitiveSceneProxy.cpp

    # Renderer View
    src/Renderer/View/Frustum.cpp
    src/Renderer/View/LightRenderData.cpp
    src/Renderer/View/RenderBatch.cpp

    # Renderer Passes
    src/Renderer/Passes/ForwardPass.cpp
    src/Renderer/Passes/OutlinePass.cpp
    src/Renderer/Passes/SSAOPass.cpp
    src/Renderer/Passes/TranslucencyPass.cpp
    src/Renderer/Passes/EditorPrimitivePass.cpp
    src/Renderer/Passes/ShadowPass.cpp
    src/Renderer/Passes/ReflectionPass.cpp
    src/Renderer/Passes/BloomPass.cpp
    src/Renderer/Passes/PostProcessPass.cpp
    src/Renderer/Passes/PresentPass.cpp

    # Renderer Resources
    src/Renderer/Resources/Shader.cpp
    src/Renderer/Resources/Mesh.cpp
    src/Renderer/Resources/Texture2D.cpp
    src/Renderer/Resources/Material.cpp
    src/Renderer/Resources/CubemapTexture.cpp
    src/Renderer/Resources/Framebuffer.cpp
    src/Renderer/Resources/InstanceBuffer.cpp
    src/Renderer/Resources/ShadowMap.cpp

    # GLAD
    ThirdParty/glad/src/glad.c

    # LodePNG
    ThirdParty/lodepng/lodepng.cpp

    # Dear ImGui core and GLFW/OpenGL backend
    ThirdParty/imgui/imgui.cpp
    ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_tables.cpp
    ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/backends/imgui_impl_glfw.cpp
    ThirdParty/imgui/backends/imgui_impl_opengl3.cpp
)

# ============================
# 创建可执行文件
# ============================
add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
 )

# ============================
# Include Path
# ============================
target_include_directories(
    ${PROJECT_NAME}

    PRIVATE

    # src
    ${SRC_DIR}

    # Core
    ${SRC_DIR}/Core

    # Editor
    ${SRC_DIR}/Editor

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
)

# Debug 配置下排除 Release CRT，解决与预编译 glfw3.lib 的 MSVCRT/MSVCRTD 混用冲突
# 该冲突会导致 Run-Time Check Failure #2（栈损坏）
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:/NODEFAULTLIB:MSVCRT>
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
    add_executable(
        FrustumTests
        tests/Renderer/View/FrustumTests.cpp
        src/Renderer/View/Frustum.cpp
        src/Renderer/Scene/PrimitiveSceneProxy.cpp
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
        src/Renderer/Scene/RenderScene.cpp
        src/Renderer/Scene/PrimitiveSceneProxy.cpp
        src/Renderer/View/Frustum.cpp
        src/Renderer/View/RenderBatch.cpp
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
        src/Renderer/View/LightRenderData.cpp
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
        src/Core/ApplicationOptions.cpp
        src/Core/InstanceGrid.cpp
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
        src/Renderer/View/RenderBatch.cpp
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

    add_executable(
        RenderPassContractTests
        tests/Renderer/Pipeline/RenderPassContractTests.cpp
    )

    target_include_directories(
        RenderPassContractTests
        PRIVATE
        ${SRC_DIR}
        ${THIRD_PARTY_DIR}/glad/include
        ${THIRD_PARTY_DIR}/cyCodeBase
    )

    add_test(NAME RenderPassContractTests COMMAND RenderPassContractTests)
endif()
