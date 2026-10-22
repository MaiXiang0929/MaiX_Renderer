# MaiX_Renderer

MaiX_Renderer 当前是一个基于 C++17 与 OpenGL 4.0 Core Profile 的实时图形与渲染引擎，面向渲染技术美术、Shader 开发与图形编程实践。项目围绕可调节的 PBR/NPR 材质、多 Pass 渲染管线和编辑器工作流展开，提供从场景提交到最终画面显示的完整实现。长期目标是以此为基础逐步发展为现代化游戏引擎；当前阶段聚焦图形与渲染能力。

**技术栈：** C++17 · OpenGL 4.0 · GLSL · CMake · Dear ImGui · ImGuizmo

[核心能力](#核心能力) · [渲染架构](#渲染架构) · [快速开始](#快速开始) · [编辑器操作](#编辑器操作)

## 核心能力

### PBR 与材质工作流

- 使用 Cook-Torrance metallic/roughness BRDF，支持 Base Color、Normal、ORM 和 Displacement 纹理。ORM 通道约定为 R=AO、G=Roughness、B=Metallic。
- 对颜色纹理与数据纹理分别使用 sRGB/Linear 语义；标准网格、Instancing 和 Tessellation/Displacement 路径共用 PBR 光照。
- 环境加载时由 GPU 生成漫反射辐照度、GGX 粗糙度预过滤 Cubemap 与 BRDF LUT；主视图、平面反射及透明 PBR 共用 IBL，浮点环境资源与反射结果由最终后处理统一曝光和色调映射。
- `--material-lab` 使用共享 Mesh 展示铜、塑料、陶瓷和粗糙金属四种材质。Material Editor 可在运行时修改材质参数与纹理。

IBL 当前使用内置六面 sRGB PNG 天空，不包含 HDR/EXR 环境文件导入或动态反射探针。预计算在加载环境或 F6 重载时执行；Roughness 控制环境反射模糊程度，Metallic 控制漫反射与镜面分配，Environment Reflectivity 保留为环境镜面强度，AO 作用于间接光。可用 `--material-lab` 对照粗糙度与金属度，并旋转视角检查反射方向；画面效果仍需用户验收。

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
    │ PrepareFrame 注册并同步 World，再由 SceneFrameBuilder 准备 RenderFrameData
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

无参数启动会重开上次成功打开的项目及其默认场景；首次启动显示内置 Ground/Monkey 示例。默认选中 Monkey，可在 Scene 中选择地面或点光源，在 Inspector/Material Editor 中编辑。点光源当前不支持阴影；SSAO 保留现有手动开关，默认关闭。

构建产物位于 `out/build/windows-ninja-debug/`；构建时部署最新的 `assets/` 和 `Shaders/`，支持从仓库根目录或可执行文件目录启动。

## 编辑器操作

默认工作区采用左侧 Scene、中央 Viewport、右侧 Inspector 的布局，左侧与中央下方由 Content Browser 和 Console 共用标签区。Material Editor 打开后继续与 Inspector 共用标签。`View → Theme → Light / Dark` 切换雾蓝与柔粉的浅色/深色主题；`View → UI Scale` 调整界面缩放，并叠加窗口所在显示器的内容缩放。主题与缩放保存在用户目录的 `MaiX_Renderer/imgui.ini` 中，切换主题不重置布局。`Reset Layout` 恢复新版默认布局。

Viewport 顶部工具栏提供移动、旋转、缩放与世界/局部空间切换；不可用于当前对象的操作会禁用。工具栏不计入视口图像的拾取和渲染范围。Inspector 中 Transform、Materials 使用折叠分组，导入尺寸和源单位位于默认收起的 Asset Info。资源浏览器只浏览当前用户项目的 `Assets/`，提供统一雾蓝文件夹图标、相对路径导航和当前目录名称过滤。下列场景快捷键在 Viewport 获得焦点时生效；鼠标场景操作从视口图像内开始。

引擎内置 `assets/`（字体、天空盒、默认模型和场景）及其构建副本与用户内容分离。`File → New Project...` 选择电脑上的可写父目录，再输入名称，创建 `名称/名称.maixproject` 与 `Assets/Scenes/Main.maixscene`；默认场景包含 Ground、Monkey、点光源和 Camera。项目不能放入引擎内置资源、Shader 或构建部署目录；同名项目目录已存在时拒绝创建，构建只部署引擎资源。

`File → Open Project...` 选择 `.maixproject`，同时加载该项目的默认场景和资源浏览目录；全部加载成功才切换，失败保留原项目、场景及资源。当前场景有未保存修改时，可选择 Save and Continue、Discard and Continue 或 Cancel；保存操作写入切换前的项目。项目文件记录格式版本、名称和相对 Assets 的默认场景路径。File 菜单显示当前项目、根目录与场景路径；Content Browser 顶部显示项目名称，悬停可查看根目录。

旧版只有 `Assets/` 的目录通过 `File → Initialize Project Folder...` 显式初始化：选择项目根目录（Assets 的上一级），再选择 Assets 内已有 `.maixscene` 作为默认场景，或创建默认 Main。已有文件不会覆盖，已有项目描述文件的目录不会重复初始化。首次启动仍兼容 Windows“文档”实际位置下的 `MaiX Projects/DefaultProject/Assets` 和旧版 Root 配置；旧配置误记默认项目的 Assets 目录时，启动会恢复到已知项目根目录，已有文件保持原位。其他误记 Assets 的配置只有找到父级项目描述文件才恢复，否则显示提示，不再创建 Assets/Assets。上次项目路径保存在用户目录的 `MaiX_Renderer/imgui.ini` 中。

场景里的 Ground/Monkey 使用内置模型引用，不会复制模型进项目。Content Browser 显示当前子目录路径，过滤框仅过滤当前目录；通过左侧目录树切换目录或返回 `Assets` 根目录。双击项目中的 FBX 可加载到当前场景；`File → Import FBX...` 也可选择外部源文件。加载操作不自动复制模型或贴图依赖。

`File → Save Scene / Save Scene As...` 保存带版本号的 `.maixscene` JSON，默认目录为当前项目 `Assets/Scenes/`，文件须保存在该项目 Assets 内；项目本身的位置可自由选择。`File → Open Scene...` 重开场景；当前内容有未保存修改时，可选择保存后打开、放弃修改或取消。打开失败保留当前场景，保存失败保留原文件。场景不会自动保存，Save As 不改变项目的默认场景设置。

场景文件保留对象名称、父子层级、局部变换、本地 Visible、模型分段与材质绑定、灯光和相机参数，以及基础材质、实例父子关系、参数覆盖和纹理继承/替换/禁用。运行时资源 Handle 不写入文件。项目内资源使用 Assets 相对引用，内置资源使用引擎引用，项目外文件保留明确的绝对路径；不自动复制或归档依赖。FBX 内嵌纹理以源 FBX 的元素身份定位，缺失或不兼容来源会报错。

`View → Render Settings` 编辑当前场景的 Exposure（EV）、ACES Tone Mapping、Bloom 和 SSAO 参数；`Reset to Defaults` 恢复默认值，关闭效果保留其参数。曝光每增加 1 EV，线性亮度加倍；Bloom Threshold 使用曝光前的线性 HDR 值，SSAO Radius/Bias 使用场景世界单位。修改即时影响画面，并纳入未保存修改检测；通过 Save Scene 保存后，再次打开该场景恢复这些值；重启仍按项目默认场景打开。参数由 CPU 场景持有，每帧以快照传入渲染管线，GPU 资源仍由 Renderer/Pass 管理。

新保存的场景使用 v2 格式。v1 场景仍可读取，并明确使用默认曝光 0 EV、开启 Tone Mapping/Bloom、关闭 SSAO 及原有默认强度；不会继承前一个场景的后处理参数。v2 参数缺失、非有限、越界或包含未知字段时拒绝加载；资源重建失败也保留当前场景和参数。阴影/细分调试开关、编辑器辅助图元、导航相机与 UI 配置不纳入本轮场景设置；UI 配置仍独立保存于 imgui.ini。Renderer Statistics 保留统计和其他已有调试控件，后处理编辑统一放在 Render Settings。

复制或搬移整个项目目录后，可在新位置打开 `.maixproject`；Assets 内相对引用会跟随项目，引擎引用仍指向内置内容，外部绝对引用仍要求原文件可访问。当前支持 FBX 模型与已有内置 Cube。含无来源生成纹理或仅供渲染验证的透明平面的场景，会明确拒绝保存；资产数据库、依赖归档、项目 Hub 与 Undo/Redo 尚未实现。

| 操作 | 用途 |
| --- | --- |
| 鼠标左键点选 | 选择模型、可见灯光或相机图标 |
| Scene 中点选模型 | 按模型独立选择；Inspector 和 Gizmo 只修改当前模型 |
| Inspector 中点击材质 | 激活同一区域的 Material Editor 标签，编辑该模型实际使用的材质 |
| Scene 中 `Delete Subtree (N)` | 删除选中对象及后代，清理组件与渲染资源 |
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

Scene 显示实际对象层级。`Create Empty` 创建组合根节点；把模型、灯光或相机拖到另一个对象上，或在 Inspector 的 `Parent` 中选择父级。换父级和 `Detach to Root` 保持世界变换。Inspector 的 `Local Transform` 编辑相对父级的位置、旋转和缩放，视口 Gizmo 编辑世界变换，`F` 可聚焦空对象。`Name` 输入后按 Enter 改名。

Inspector 的 `Visible` 控制对象自己的显示开关；父级隐藏会作用于整棵子树，Scene 树用 `(hidden)` 标识有效隐藏对象，`Hidden by parent` 表示开关虽开启但被祖先隐藏。恢复父级不会覆盖子对象自己的开关。隐藏模型不进入主视图、反射或阴影，隐藏灯光不参与照明与阴影；隐藏对象不显示辅助图标/Gizmo，也不参与视口拾取，仍可在 Scene 树选中并通过 Inspector 编辑。相机隐藏只影响编辑图标，其取景仍可使用。

隐藏不会卸载模型或材质，也不会取消资源引用保护。隐藏期间的变换继续由 World 同步，恢复后保留最新位置。EngineSession 的 `PrepareFrame` 统一完成注册、状态同步与多视图帧准备，失效 Actor/组件绑定会拒绝该帧输出。

父级修改会同步子模型、灯光、相机、阴影、包围体和拾取。当前支持能表示为 TRS 的层级变换；非均匀缩放配合旋转若产生剪切，或输入非有限/奇异变换，操作会完整拒绝并显示原因。`Delete Subtree (N)` 删除选中对象及全部后代，并清理渲染引用；删除活动场景相机后返回编辑器视图。

Material Editor 默认跟随所选模型，显示所属模型名称和材质列表。`All materials (debug)` 可查看全部场景材质；点击 Inspector 中的材质会返回模型范围。删除正在编辑的模型后会清理材质选择。

基础材质可通过 `Create Instance` 创建并绑定到当前模型分段。实例支持参数 Override、纹理继承/替换/禁用、重置覆盖和 `Edit Parent`；`Use Parent Material` 恢复基础材质。使用 `--material-instance-lab` 启动父材质与两个实例的共享 Mesh 验收场景。

## 项目结构

```text
Engine/
├─ Core/                 CPU 数学、平台窗口与文件对话框
├─ Runtime/
│  ├─ Component/         编辑器导航 Camera
│  ├─ Scene/             World/Actor 层级、模型/灯光/相机组件、启动数据
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
