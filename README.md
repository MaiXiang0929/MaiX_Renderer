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

固定管线通过显式资源契约声明各 Pass 的输入、输出与附件清空/保留行为；执行前校验本帧资源、尺寸和格式，输出发布后才允许后续 Pass 消费。Framebuffer 仍由各 Pass 管理，不额外复制主场景颜色。Renderer 持有 RHIDevice，资源包装与全部 Pass 通过类型化句柄和即时 RHICommandList 提交到 OpenGL 后端。后端负责 GPU 资源、绘制、状态与诊断；编辑器通过受限原生桥接将纹理交给 ImGui。失败帧停止后续 Pass，不向视口返回旧的最终纹理。当前仍只有 OpenGL 后端，不包含延迟命令或跨 API 实现。

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

无参数启动会载入默认场景。默认选中 Monkey，可在 Scene 中选择地面或点光源，在 Inspector/Material Editor 中编辑。点光源当前不支持阴影；SSAO 保留现有手动开关，默认关闭。

构建产物位于 `out/build/windows-ninja-debug/`；构建时部署最新的 `assets/` 和 `Shaders/`，支持从仓库根目录或可执行文件目录启动。

## 编辑器操作

默认工作区采用左侧 Scene、中央 Viewport、右侧 Inspector 的布局，左侧与中央下方由 Content Browser 和 Console 共用标签区。Material Editor 打开后继续与 Inspector 共用标签。`View → Theme → Light / Dark` 切换雾蓝与柔粉的浅色/深色主题；`View → UI Scale` 调整界面缩放，并叠加窗口所在显示器的内容缩放。主题与缩放保存在用户目录的 `MaiX_Renderer/imgui.ini` 中，切换主题不重置布局。`Reset Layout` 恢复新版默认布局。

Viewport 顶部工具栏提供移动、旋转、缩放与世界/局部空间切换；不可用于当前对象的操作会禁用。工具栏不计入视口图像的拾取和渲染范围。Inspector 中 Transform、Materials 使用折叠分组，导入尺寸和源单位位于默认收起的 Asset Info。资源浏览器只浏览当前用户项目的 `Assets/`，提供统一雾蓝文件夹图标、相对路径导航和当前目录名称过滤。下列场景快捷键在 Viewport 获得焦点时生效；鼠标场景操作从视口图像内开始。

引擎内置 `assets/`（字体、天空盒、默认模型和场景）及其构建副本与用户内容分离。首次启动在 Windows 的“文档”实际位置下创建 `MaiX Projects/DefaultProject/Assets`；文档目录重定向时同样生效。通过 `File → Open Project Folder...` 选择其他项目根目录，并创建其中缺失的 `Assets/`。上次成功选择保存在用户目录的 `MaiX_Renderer/imgui.ini` 的项目配置段中；失效配置显示错误，不会改为浏览引擎资源。项目内容不能放入引擎内置资源或构建部署目录，构建只部署引擎资源。

默认用户目录最初为空；场景里的 Ground/Monkey 是内置示例，不会自动复制进去。Content Browser 顶部显示项目名称与当前子目录路径，过滤框仅过滤当前目录；通过左侧目录树切换目录或返回 `Assets` 根目录。双击项目中的 FBX 可加载到当前场景；`File → Import FBX...` 也可选择外部源文件。加载操作不自动复制模型或贴图依赖。切换项目内容目录保留当前场景，暂不提供项目场景保存、资产数据库或依赖归档。

| 操作 | 用途 |
| --- | --- |
| 鼠标左键点选 | 选择模型、可见灯光或相机图标 |
| Scene 中点选模型 | 按模型独立选择；Inspector 和 Gizmo 只修改当前模型 |
| Inspector 中点击材质 | 激活同一区域的 Material Editor 标签，编辑该模型实际使用的材质 |
| Scene 中 `Remove selected model` | 移除选中模型及其 Primitive、Mesh、Material 资源 |
| `Alt` + 左键拖动 | Orbit 相机 |
| `Alt` + 中键拖动 | Pan 相机 |
| `Alt` + 右键拖动或滚轮 | Dolly 相机 |
| `Ctrl` + 左键拖动 | 旋转主灯光朝向，对方向光和聚光灯有效；默认点光源使用 Gizmo 移动 |
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
   ├─ RHI/OpenGL/        即时资源、绘制、状态和诊断后端
   ├─ Renderer/          Core、Scene、View、Pipeline、Resources、Diagnostics
   ├─ Material/          Material、Instance 与有效材质解析
   └─ Passes/            已有渲染算法
Editor/                  EditorApp、Viewport、场景/属性/材质/导入面板与 UI 会话
Sandbox/                 不链接 ImGui 的引擎/真实 OpenGL 验证宿主
Shaders/                 按 PBR、NPR、阴影、后处理等用途组织的 GLSL
assets/                  引擎内置字体、图片、模型与默认启动场景
tests/                   数学、资产、渲染、交互和架构检查
ThirdParty/              第三方依赖
docs/                    仅本地保留，已取消 Git 跟踪
```


## 许可证与资产署名

第三方依赖的许可证信息请查看其目录中的 LICENSE/README 文件。
