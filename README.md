# MaiX Renderer

MaiX Renderer 是一个基于 C++17 与 OpenGL 4.0 Core Profile 的实时光栅化渲染器，面向渲染技术美术、Shader 开发与图形编程实践。项目围绕可调节的 PBR/NPR 材质、多 Pass 渲染管线和编辑器工作流展开，提供从场景提交到最终画面显示的完整实现。

**技术栈：** C++17 · OpenGL 4.0 · GLSL · CMake · Dear ImGui · ImGuizmo

[核心能力](#核心能力) · [渲染架构](#渲染架构) · [快速开始](#快速开始) · [编辑器操作](#编辑器操作) · [延伸文档](#延伸文档)

## 核心能力

### PBR 与材质工作流

- 使用 Cook-Torrance metallic/roughness BRDF，支持 Base Color、Normal、ORM 和 Displacement 纹理。ORM 通道约定为 R=AO、G=Roughness、B=Metallic。
- 对颜色纹理与数据纹理分别使用 sRGB/Linear 语义；标准网格、Instancing 和 Tessellation/Displacement 路径共用 PBR 光照。
- `--material-lab` 使用共享 Mesh 展示铜、塑料、陶瓷和粗糙金属四种材质。Material Editor 可在运行时修改材质参数与纹理。

### NPR 与透明渲染

- Toon 材质支持分段漫反射、阴影色和 Rim Light；独立的 Outline Pass 使用 Inverted Hull 绘制轮廓。
- Face Shadow 使用线性 SDF 纹理、角色局部 Face Forward/Right、左右镜像和独立 Key Light；可通过 `--face-shadow-demo` 加载外部 FBX 与面部阴影贴图。
- Translucency Pass 按视图深度从后向前绘制 Alpha Blend 物体，使用纹理 Alpha 并保持深度只读。

### 阴影、HDR 与后处理

- Directional/Spot Light 阴影贴图与 PCF 采样；Forward Shader 使用 UBO 读取场景灯光。
- Forward Pass 写入 RGBA16F HDR Scene Color；Bloom、SSAO、手动曝光、ACES Tone Mapping 和 sRGB 输出编码组成后处理链路。
- 支持 Cubemap、半分辨率反射、离屏渲染目标和随窗口尺寸变化的目标重建。

### 场景与编辑器

- RenderScene 使用 Primitive/Light Scene Proxy 构建主视图、反射视图和阴影视图；CPU 侧执行包围球视锥裁剪、不透明资源排序与 Instancing 批次准备。
- Renderer 持有 Mesh/Material 资源，场景通过强类型 Handle 提交共享资源。FBX 导入支持多 Mesh、多材质分段、索引绘制、纹理和异步 CPU 解析。
- ImGui 提供 Scene Window、Inspector、Material Editor 和 Renderer Statistics；视口支持相机导航、模型/灯光拾取与 ImGuizmo Transform。
- GPU Debug Group、每 Pass 提交统计和 GPU Timer Query 辅助定位渲染开销。

## 渲染架构

```text
Application（输入、相机、可编辑场景状态）
    │ 提交 RenderFrameData
    ▼
Renderer（GPU 资源所有者）
    ├─ RenderScene：Primitive / Light Scene Proxy
    │      └─ BuildRenderView() → RenderItem 列表
    ├─ Resources：Mesh / Material / Shader / Texture / Framebuffer
    └─ RenderPipeline
         Shadow → Reflection → Forward → Outline → Translucency
               → SSAO → Bloom → PostProcess → EditorPrimitive → Present
    ▼
OpenGL GPU → 窗口
```

每帧由 `Application` 更新相机、模型 Transform 和灯光，`RenderScene::BuildRenderView()` 生成各视图的可见项。CPU 负责裁剪、排序、矩阵与资源绑定准备；GPU 执行顶点变换、光栅化、深度与混合测试、纹理采样、光照和像素输出。各 Pass 依次消费场景视图与前序 Pass 生成的纹理资源。

## 快速开始

### 环境要求

- Windows 10/11 x64；
- Visual Studio 2022 或兼容的 MSVC x64 工具链，以及 Ninja；
- CMake 3.23 或更高版本；
- 支持 OpenGL 4.0 Core Profile 的显卡和驱动。

GLFW、GLAD、ufbx、cyCodeBase、LodePNG、Dear ImGui 和 ImGuizmo 等依赖位于 `ThirdParty/`。

### 构建与运行

在仓库根目录打开 Visual Studio Developer PowerShell (x64)：

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset windows-ninja-debug
.\out\build\windows-ninja-debug\OpenGL_Project.exe
```

构建产物位于 `out/build/windows-ninja-debug/`；构建时会将最新的 `assets/` 复制到可执行文件目录。

### 常用运行入口

以下命令均从仓库根目录执行：

```powershell
# PBR 材质场景
.\out\build\windows-ninja-debug\OpenGL_Project.exe --material-lab

# 透明材质场景
.\out\build\windows-ninja-debug\OpenGL_Project.exe --translucency-test

# N×N 共享资源 Instancing 场景，N 为 1–32
.\out\build\windows-ninja-debug\OpenGL_Project.exe --instance-grid 16

# 查看完整命令行参数
.\out\build\windows-ninja-debug\OpenGL_Project.exe --help
```

Face Shadow 入口需要用户提供 FBX、连续变化的面部阴影贴图，以及 FBX 中的精确材质名称；外部角色资产不包含在仓库中：

```powershell
.\out\build\windows-ninja-debug\OpenGL_Project.exe --face-shadow-demo `
  "<角色模型.fbx>" "<面部阴影贴图.png>" "<材质名称>"
```

运行时也可使用 `File > Import FBX...` 导入静态模型。项目通过异步 CPU 解析和 GPU 资源创建替换当前模型。

### 测试

```powershell
ctest --test-dir out/build/windows-ninja-debug --output-on-failure
```

## 编辑器操作

| 操作 | 用途 |
| --- | --- |
| 鼠标左键点选 | 选择模型或可见灯光 Gizmo |
| `Alt` + 左键拖动 | Orbit 相机 |
| `Alt` + 中键拖动 | Pan 相机 |
| `Alt` + 右键拖动或滚轮 | Dolly 相机 |
| `Ctrl` + 左键拖动 | 旋转主 Spot Light |
| `F` | 聚焦当前选中对象 |
| `W` / `E` / `R` | ImGuizmo 移动 / 旋转 / 缩放 |
| `Q` | 切换世界/局部 Transform 空间 |
| `F6` | 重新加载 GLSL Shader |
| `Esc` | 退出程序 |

## 项目结构

```text
src/
├─ Assets/              FBX 与图片导入
├─ Core/                Application、Camera、Input、Frame 数据
├─ Editor/              ImGui 面板、拾取与 Gizmo
├─ Platform/            平台相关接口
└─ Renderer/
   ├─ Core/             Renderer 入口与 GPU 资源所有权
   ├─ Diagnostics/      GPU Timer、Debug Group 与提交统计
   ├─ Pipeline/         RenderPipeline、Pass 契约与设置
   ├─ Passes/           阴影、前向、NPR、后处理与显示
   ├─ Resources/        Mesh、Material、Shader、Texture、Framebuffer
   ├─ Scene/            RenderScene 与 Scene Proxy
   └─ View/             RenderView、RenderItem、Batch、Frustum

assets/shaders/          按 PBR、NPR、阴影、后处理等用途组织的 GLSL
docs/                    各子系统的数据流、设计与验证记录
tests/                   数学、导入器、资源与交互测试
ThirdParty/              项目使用的第三方依赖
```

## 延伸文档

| 主题 | 文档 |
| --- | --- |
| PBR 材质与纹理约定 | [PBR Material Workflow](docs/pbr-material-workflow.md) |
| Toon、Face Shadow、Outline | [Toon Shading](docs/npr-toon.md) · [Face Shadow](docs/npr-face-shadow.md) · [Outline](docs/npr-outline.md) |
| 透明渲染 | [Translucency Pass](docs/translucency-pass.md) |
| HDR、Bloom、SSAO | [HDR and Tone Mapping](docs/hdr-tone-mapping.md) · [Bloom and Post Process](docs/bloom-postprocess.md) · [SSAO](docs/ssao.md) |
| 编辑器工作流 | [Scene Window and Inspector](docs/editor-scene-inspector.md) · [Viewport Transform Controls](docs/viewport-transform-controls.md) |
| 性能分析 | [GPU Pass Profiling](docs/gpu-pass-profiling.md) · [RenderDoc Baseline](docs/renderdoc-baseline.md) |

## 许可证与资产署名

项目代码和第三方依赖的许可证信息请查看对应目录中的 LICENSE/README 文件。外部角色资产不属于本仓库；使用或分发时应保留其原始署名与来源说明。
