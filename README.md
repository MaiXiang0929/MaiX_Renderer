# MaiX_Renderer

MaiX_Renderer 当前是一个基于 C++17 与 OpenGL 4.0 Core Profile 的实时图形与渲染引擎，面向渲染技术美术、Shader 开发与图形编程实践。项目围绕可调节的 PBR/NPR 材质、多 Pass 渲染管线和编辑器工作流展开，提供从场景提交到最终画面显示的完整实现。长期目标是以此为基础逐步发展为现代化游戏引擎；当前阶段聚焦图形与渲染能力。

**技术栈：** C++17 · OpenGL 4.0 · GLSL · CMake · Dear ImGui · ImGuizmo

[核心能力](#核心能力) · [渲染架构](#渲染架构) · [快速开始](#快速开始) · [编辑器操作](#编辑器操作)

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
EditorApp（输入、编辑器相机与面板） / Sandbox（独立验证宿主）
    ▼
EngineSession（场景状态与 Renderer 生命周期）
    │ SceneFrameBuilder 准备 RenderFrameData
    ▼
Renderer（GPU 资源所有者）
    ├─ RenderScene：Primitive / Light Scene Proxy
    │      └─ BuildRenderView() → RenderItem 列表
    ├─ Resources：Mesh / Material / Shader / Texture / Framebuffer
    └─ RenderPipeline
         Shadow → Reflection → Forward → Outline → Translucency
               → SSAO → Bloom → PostProcess → EditorPrimitive → Present
    ▼
OpenGL GPU → 最终纹理 → 编辑器 Viewport
```

每帧由 EditorApp 收集输入与编辑操作，EngineSession 持有场景数据，SceneFrameBuilder 准备多视图帧数据，`RenderScene::BuildRenderView()` 生成各视图的可见项。CPU 负责裁剪、排序、矩阵与资源绑定准备；GPU 执行顶点变换、光栅化、深度与混合测试、纹理采样、光照和像素输出。各 Pass 依次消费场景视图与前序 Pass 生成的纹理资源。

固定管线通过显式资源契约声明各 Pass 的输入、输出与附件清空/保留行为；执行前校验本帧资源、尺寸和格式，输出发布后才允许后续 Pass 消费。Framebuffer 仍由各 Pass 管理，不额外复制主场景颜色。Present 的纹理绑定与绘制通过最小 RHICommandList 转入 OpenGL 后端，其余资源与 Pass 的完整 RHI 迁移留待 Phase 1。

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
.\out\build\windows-ninja-debug\MaiX_Renderer.exe
```

无参数启动会载入 Cube、Directional Light、Camera：1 单位=1 米，正方体边长 2 米，中心位于原点，默认材质不绑定贴图。启动场景 JSON 位于 assets/scenes/startup.json，加载失败时使用由同一文件生成的内嵌回退。

构建产物位于 `out/build/windows-ninja-debug/`；构建时部署最新的 `assets/` 和 `Shaders/`，支持从仓库根目录或可执行文件目录启动。

## 编辑器操作

默认工作区左侧上方是 Viewport，左下方由 Console 与 Content Browser 共用标签区，右侧是并排的 Scene 和 Inspector。Material Editor 打开后与 Inspector 共用最右侧区域，通过标签切换。`View` 菜单可打开 Material Editor、Renderer Statistics，或用 `Reset Layout` 恢复默认布局。下列场景快捷键在 Viewport 获得焦点时生效；鼠标场景操作从视口图像内开始。

| 操作 | 用途 |
| --- | --- |
| 鼠标左键点选 | 选择模型、可见灯光或相机图标 |
| Scene 中点选模型 | 按模型独立选择；Inspector 和 Gizmo 只修改当前模型 |
| Inspector 中点击材质 | 激活同一区域的 Material Editor 标签，编辑该模型实际使用的材质 |
| Scene 中 `Remove selected model` | 移除选中模型及其 Primitive、Mesh、Material 资源 |
| `Alt` + 左键拖动 | Orbit 相机 |
| `Alt` + 中键拖动 | Pan 相机 |
| `Alt` + 右键拖动或滚轮 | Dolly 相机 |
| `Ctrl` + 左键拖动 | 旋转主方向光 |
| 小键盘 `0` | 切换场景 Camera / 编辑器视图 |
| `F` | 聚焦当前选中对象 |
| `W` / `E` / `R` | ImGuizmo 移动 / 旋转 / 缩放 |
| `Q` | 切换世界/局部 Transform 空间 |
| `F6` | 重新加载 GLSL Shader |
| `Esc` | 退出程序 |

Material Editor 默认跟随所选模型，显示所属模型名称和材质列表。`All materials (debug)` 可查看全部场景材质；点击 Inspector 中的材质会返回模型范围。删除正在编辑的模型后会清理材质选择。

基础材质可通过 `Create Instance` 创建并绑定到当前模型分段。实例支持参数 Override、纹理继承/替换/禁用、重置覆盖和 `Edit Parent`；`Use Parent Material` 恢复基础材质。使用 `--material-instance-lab` 启动父材质与两个实例的共享 Mesh 验收场景。

## 项目结构

```text
Engine/
├─ Core/                 CPU 数学、平台窗口与文件对话框
├─ Runtime/
│  ├─ Component/         现有 Camera；完整组件体系留待 Phase 2
│  ├─ Scene/             CPU 模型、灯光、场景相机、启动数据
│  └─ Engine/            EngineSession、SceneState、RenderSceneBridge、SceneFrameBuilder
├─ Asset/
│  ├─ Geometry/          CPU Vertex 与程序化几何
│  └─ Importer/          FBX/Image CPU 解析
└─ Rendering/
   ├─ RHI/OpenGL/        首个即时 Present 命令入口
   ├─ Renderer/          Core、Scene、View、Pipeline、Resources、Diagnostics
   ├─ Material/          Material、Instance 与有效材质解析
   └─ Passes/            已有渲染算法
Editor/                  EditorApp、Viewport、场景/属性/材质/导入面板与 UI 会话
Sandbox/                 不链接 ImGui 的引擎/真实 OpenGL 验证宿主
Shaders/                 按 PBR、NPR、阴影、后处理等用途组织的 GLSL
assets/                  字体、图片、默认启动场景等非 Shader 资源
tests/                   数学、资产、渲染、交互和架构检查
ThirdParty/              第三方依赖
docs/                    仅本地保留，已取消 Git 跟踪
```

## 验证与阶段边界

```powershell
ctest --test-dir out/build/windows-ninja-debug --output-on-failure
.\out\build\windows-ninja-debug\MaiX_Sandbox.exe
```

Sandbox 使用隐藏窗口验证真实 OpenGL 像素、共享资源释放与引擎生命周期，不链接 ImGui/ImGuizmo。M0 已整理模块归属并拆出平台、引擎和编辑器职责；当前固定管线、资源句柄和效果保留。SceneState 暂时保留 CPU 渲染绑定契约，不等同于完整 World/Actor/Component；RHI 仅接通 Present 命令，完整资源抽象在 M1，Render Graph 在 M4。

本地 docs 仅保留[当前执行方案](docs/execution-plan.md)、[主要功能实现](docs/implementation.md)和[项目文件结构](docs/project-structure.md)，不随 Git 克隆或同步。方案由用户要求时更新；新增主要功能后询问是否更新实现说明；项目文件变化时同步更新结构清单。构建和测试通过不代表画面与交互已验收，视觉效果由用户确认。

## 许可证与资产署名

第三方依赖的许可证信息请查看其目录中的 LICENSE/README 文件。外部角色资产不属于本仓库；使用或分发时应保留其原始署名与来源说明。
