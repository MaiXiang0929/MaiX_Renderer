# MaiX_Renderer

MaiX_Renderer 当前是一个基于 C++17 与 OpenGL 4.0 Core Profile 的实时图形与渲染引擎，面向渲染技术美术、Shader 开发与图形编程实践。项目围绕可调节的 PBR/NPR 材质、多 Pass 渲染管线和编辑器工作流展开，提供从场景提交到最终画面显示的完整实现。长期目标是以此为基础逐步发展为现代化游戏引擎；当前阶段聚焦图形与渲染能力。

**技术栈：** C++17 · OpenGL 4.0 · GLSL · CMake · Dear ImGui · ImGuizmo

[核心能力](#核心能力) · [渲染架构](#渲染架构) · [快速开始](#快速开始) · [编辑器操作](#编辑器操作)

## 核心能力

### PBR 与材质工作流

- 使用 Cook-Torrance metallic/roughness BRDF，支持 Base Color、Normal、ORM 和 Displacement 纹理。ORM 通道约定为 R=AO、G=Roughness、B=Metallic。
- 对颜色纹理与数据纹理分别使用 sRGB/Linear 语义；标准网格、Instancing 和 Tessellation/Displacement 路径共用 PBR 光照。
- 环境加载时由 GPU 生成漫反射辐照度、GGX 粗糙度预过滤 Cubemap 与 BRDF LUT；主视图、平面反射及透明 PBR 共用 IBL，浮点环境资源与反射结果由最终后处理统一曝光和色调映射。
- 材质支持 Front、Back、Double 与 Legacy 面模式；新材质默认 Front，实例继承父材质。主视图、反射、透明和阴影统一处理剔除及负缩放绕序，Double 背面使用翻转后的完整法线贴图法线。
- `--material-lab` 使用共享 Mesh 展示铜、塑料、陶瓷和粗糙金属四种材质。Material Editor 可在运行时修改材质参数与纹理。

IBL 可使用内置六面 sRGB PNG 天空或 Radiance `.hdr` 全景；不包含 EXR、动态天空或动态反射探针。HDR 在加载时转换为浮点 Cubemap 并生成 IBL，F6 重载会重新生成当前环境的 IBL；Roughness 控制环境反射模糊程度，Metallic 控制漫反射与镜面分配，Environment Reflectivity 保留为环境镜面强度，AO 作用于间接光。可用 `--material-lab` 对照粗糙度与金属度，并旋转视角检查反射方向；既有 HDR/IBL 效果已由用户验收。

### NPR 与透明渲染

- Toon 材质支持分段漫反射、阴影色和 Rim Light；独立的 Outline Pass 使用 Inverted Hull 绘制轮廓。
- Face Shadow 使用线性 SDF 纹理、角色局部 Face Forward/Right、左右镜像和独立 Key Light；可通过 `--face-shadow-demo` 加载外部 FBX 与面部阴影贴图。
- Masked 材质使用 Base Color Alpha × Opacity 与 Alpha Cutoff（默认 0.5）裁剪，低于阈值的片元不写颜色和深度；保留片元按不透明表面绘制。主视图、平面反射、阴影及普通/实例化/细分路径使用同一规则，NPR 描边也执行裁剪。Material Editor 的 Blend Mode 选择 Masked，实例可单独覆盖 Cutoff；模式继承父材质。参数随场景保存，旧场景缺少 Cutoff 时使用 0.5。剔除由 Surface Side 决定；拾取仍使用几何，不做 Alpha 像素拾取。
- Translucency Pass 按视图深度从后向前绘制 Alpha Blend 物体，使用纹理 Alpha 并保持深度只读。

### 阴影、HDR 与后处理

- 最多 4 盏方向光、聚光灯或点光源同时投影，来自现有 Forward 最多 16 盏可见灯光。符合条件的主灯优先，其余按场景灯光顺序分配；超预算灯仍照明，Render Settings 的 Shadows 显示分配状态。方向光/聚光灯使用二维深度数组，点光源使用六面径向深度 Cubemap 数组，资源均归 ShadowPass 所有。
- 聚光灯按世界位姿、两倍 Outer Cone 半角和 Range 生成透视阴影；点光源按世界位置生成六个 90° 阴影视图，近面均为 Range 的 0.1%。方向光可选 1/2/4 级：单级按场景范围拟合，多级按当前接收视图拟合、纹素对齐、交界混合及末级淡出；平面反射使用独立级联。普通、Masked、Instancing 与 Tessellation/Displacement 路径共用覆盖规则，PBR/Toon 的各灯直接光独立接收阴影，透明表面也可接收。镜面地面保留主灯的艺术化阴影调制。已有聚光灯和灯光编辑已验收，本轮新增阴影效果待用户验收。
- FXAA 在 Present 阶段处理色调映射和 sRGB 编码后的场景颜色，再合成编辑器叠加层；无需新增全分辨率目标。可在 Render Settings 的 Anti-aliasing → Method 选择 Off/FXAA。新建场景默认 FXAA，旧 v1/v2 场景缺少该字段时使用 Off；它可平滑轮廓和描边，也可能软化细节，不能消除运动或高光的时域闪烁。
- Forward Pass 写入 RGBA16F HDR Scene Color；Bloom、SSAO、手动曝光、ACES Tone Mapping 和 sRGB 输出编码组成后处理链路。
- SSAO 只衰减 PBR 的间接漫反射与 Toon 的基础环境项，保留直接光、环境镜面、Rim、天空和镜面地面反射。独立 RGBA16F 附件记录可遮蔽贡献；描边覆盖时清零，透明混合时按透过率衰减背景贡献，透明层自身不使用主视图 SSAO。材质 ORM AO 的原有作用保持不变。附加目标随视口重建，当前即使 SSAO 关闭也保留分配（8 字节/像素）。
- 支持 Cubemap、半分辨率反射、离屏渲染目标和随窗口尺寸变化的目标重建。

### 场景与编辑器

- RenderScene 使用 Primitive/Light Scene Proxy 构建主视图、反射视图和阴影视图；CPU 侧执行包围球视锥裁剪、不透明资源排序与 Instancing 批次准备。
- Renderer 持有 Mesh/Material 资源，场景通过强类型 Handle 提交共享资源。FBX 导入支持多 Mesh、多材质分段、索引绘制和异步 CPU 解析；Base Color、Normal 与灰度 AO/Roughness/Metallic 输入自动绑定，分离数据图合成为 Linear ORM，保留材质倍率。
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

无参数启动会重开上次成功打开的项目及其默认场景；首次启动显示内置 Ground/Monkey 示例。默认选中 Monkey，可在 Scene 中选择地面或点光源，在 Inspector/Material Editor 中编辑。默认点光源的 Cast Shadow 关闭，可在 Inspector 开启；SSAO 默认关闭。

构建产物位于 `out/build/windows-ninja-debug/`；构建时部署最新的 `assets/` 和 `Shaders/`，支持从仓库根目录或可执行文件目录启动。

## 编辑器操作

默认工作区采用左侧 Scene、中央 Viewport、右侧通高 Inspector 的布局，左侧与中央下方由 Content Browser 和 Console 共用标签区。默认左右面板约占总宽度的 16.5% 和 30.5%，底部资源区约占工作区高度的 42%；仍可拖拽调整。Material Editor 打开后继续与 Inspector 共用标签。`View → Theme → Light / Dark` 切换浅色/深色主题；`View → UI Scale` 调整界面缩放，并叠加窗口所在显示器的内容缩放。主题与缩放保存在用户目录的 `MaiX_Renderer/imgui.ini` 中，切换主题不重置布局。已有布局继续保留，`View → Reset Layout` 应用新的默认比例。菜单栏右侧显示项目名称，底部状态栏显示当前选择与项目。

Viewport 顶部工具栏提供移动、旋转、缩放与世界/局部空间切换，宽度允许时显示操作文字；不可用于当前对象的操作会禁用。工具栏不计入视口图像的拾取和渲染范围。Scene 顶部 `...` 菜单提供 Create Empty、Create Light、Detach to Root 与 Delete Subtree，删除项显示包含自身的子树对象数；对象树支持拖拽换父级。Inspector 顶部显示对象身份与 Visible，Name/Parent 使用左侧标签，Local Transform、Materials 与灯光/相机参数分组显示；导入尺寸和源单位位于默认收起的 Asset Info。资源浏览器只浏览当前用户项目的 `Assets/`，路径与筛选默认共用一行，空间不足时筛选另起一行。目录树、文件图标和 FBX 双击导入沿用原有交互。下列场景快捷键在 Viewport 获得焦点时生效；鼠标场景操作从视口图像内开始。

引擎内置 `assets/`（字体、天空盒、默认模型和场景）及其构建副本与用户内容分离。`File → New Project...` 选择电脑上的可写父目录，再输入名称，创建 `名称/名称.maixproject` 与 `Assets/Scenes/Main.maixscene`；默认场景包含 Ground、Monkey、点光源和 Camera。项目不能放入引擎内置资源、Shader 或构建部署目录；同名项目目录已存在时拒绝创建，构建只部署引擎资源。

`File → Open Project...` 选择 `.maixproject`，同时加载该项目的默认场景和资源浏览目录；全部加载成功才切换，失败保留原项目、场景及资源。当前场景有未保存修改时，可选择 Save and Continue、Discard and Continue 或 Cancel；保存操作写入切换前的项目。项目文件记录格式版本、名称和相对 Assets 的默认场景路径。File 菜单显示当前项目、根目录与场景路径；Content Browser 顶部显示项目名称，悬停可查看根目录。

旧版只有 `Assets/` 的目录通过 `File → Initialize Project Folder...` 显式初始化：选择项目根目录（Assets 的上一级），再选择 Assets 内已有 `.maixscene` 作为默认场景，或创建默认 Main。已有文件不会覆盖，已有项目描述文件的目录不会重复初始化。启动恢复与目录打开共用路径解析：选中已知默认项目、当前项目或带描述文件项目的 Assets 及其子目录时，定位所属项目；描述项目仍须场景加载成功才切换身份。未知的 Assets 根目录拒绝打开或初始化，提示选择上一级项目根目录；已有描述文件、名称恰好为 Assets 的项目仍可正常打开。新建与初始化也拒绝在已识别项目的资产目录内嵌套创建项目。仅仅有某个祖先目录叫 Assets 不会禁止在其中存放独立项目。历史嵌套目录不会自动迁移或删除。首次启动仍兼容 Windows“文档”实际位置下的 `MaiX Projects/DefaultProject/Assets`；上次项目路径保存在用户目录的 `MaiX_Renderer/imgui.ini` 中。

场景里的 Ground/Monkey 使用内置模型引用，不会复制模型进项目。Content Browser 显示当前子目录路径，过滤框仅过滤当前目录；通过左侧目录树切换目录或返回 `Assets` 根目录。双击项目中的 FBX 可加载到当前场景；`File → Import FBX...` 也可选择外部源文件。加载操作不自动复制模型或贴图依赖。

`File → Save Scene / Save Scene As...` 保存带版本号的 `.maixscene` JSON，默认目录为当前项目 `Assets/Scenes/`，文件须保存在该项目 Assets 内；项目本身的位置可自由选择。`File → Open Scene...` 重开场景；当前内容有未保存修改时，可选择保存后打开、放弃修改或取消。打开失败保留当前场景，保存失败保留原文件。场景不会自动保存，Save As 不改变项目的默认场景设置。

场景文件保留对象名称、父子层级、局部变换、本地 Visible、模型分段与材质绑定、灯光和相机参数，以及基础材质、实例父子关系、参数覆盖和纹理继承/替换/禁用。运行时资源 Handle 不写入文件。项目内资源使用 Assets 相对引用，内置资源使用引擎引用，项目外文件保留明确的绝对路径；不自动复制或归档依赖。FBX 内嵌纹理以源 FBX 的元素身份定位，缺失或不兼容来源会报错。

`View → Render Settings → Environment Lighting` 中使用 `Load HDR...` 选择 2:1 Radiance `.hdr` 全景，`Use built-in sky` 恢复内置环境，`Reload HDR` 重新读取当前文件。Environment intensity 是 0–16 的线性倍率，Environment rotation 是绕世界 Y 轴的水平旋转（-180°–180°）；天空、PBR（含透明）和反射视图统一使用，直接灯光与 Toon 原有基础环境项不受该倍率影响。地面只对环境 fallback 采样应用倍率，已有平面反射结果不重复放大。`Reset environment parameters` 恢复强度 1、旋转 0，不改变来源；上方 `Reset to Defaults` 只重置后处理和抗锯齿。

环境来源和参数随场景保存，旧场景缺少环境记录时使用内置天空、强度 1、旋转 0。项目 Assets 中的 HDR 使用相对引用，外部文件保留绝对引用；选择文件不会自动复制。环境和 IBL 重建全部成功后才提交，失败保留原场景、来源与 GPU 环境。首次加载/重载会同步解码并预计算，可能短暂等待；调整旋转与强度只更新当帧值，不重复预计算。

HDR 文件及单份解码 RGBA 浮点缓冲分别限制为 256 MiB，要求 2:1，超出 RGBA16F 范围（65504）的输入、负值或非有限值拒绝加载；解码时会同时持有编码数据和浮点候选，峰值内存高于单缓冲上限。源 Cubemap 自动选择不超过 1024 的面尺寸。强度导致输出超出半精度范围时饱和至 65504，最终仍由既有曝光与 Tone Mapping 输出。既有 HDR 环境效果已由用户验收。

`View → Render Settings` 编辑当前场景的 Exposure（EV）、ACES Tone Mapping、Bloom、SSAO 与抗锯齿设置；`Reset to Defaults` 恢复默认值（包括开启 FXAA），关闭效果保留其参数。曝光每增加 1 EV，线性亮度加倍；Bloom Threshold 使用曝光前的线性 HDR 值，SSAO Radius/Bias 使用场景世界单位。修改即时影响画面，并纳入未保存修改检测；通过 Save Scene 保存后，再次打开该场景恢复这些值；重启仍按项目默认场景打开。参数由 CPU 场景持有，每帧以快照传入渲染管线，GPU 资源仍由 Renderer/Pass 管理。

新保存的场景使用 v5 格式，记录 Normal 转换和 ORM 合成来源配方，保留 v4 的 Surface Side，以及全局阴影范围/预算/分辨率及各灯 Bias、级联数、距离和分割系数。旧 v4 场景的面模式保持原值；v1–v3 场景的材质迁移为 Legacy，保留原先不剔除与不翻转背面法线的表现；旧 v3 阴影配置保持原值。v1/v2 场景仍可读取，迁移为单主灯阴影、2048² 单图、方向光单级、点光源 Cast Shadow 关闭，保持旧画面；需要新能力时在 Render Settings 的 Shadows 开启 Multiple shadow lights，再编辑灯光参数。v1 使用原有后处理默认值；FXAA 在 v1 及缺少该字段的旧 v2 场景中关闭。非法、越界、缺失必需字段或未知字段拒绝加载，候选失败保留当前场景和参数。级联调试色、深度预览、全局阴影调试开关、细分调试、导航相机与 UI 配置不写入场景。

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

`Scene → Create Light` 可创建 `Directional`（方向光）、`Point`（点光源）或 `Spot`（聚光灯），创建后自动选中。新灯是根对象，位于场景记录中心上方 3 米，局部 -Z 默认朝下；可用 `Local Transform` 或 Gizmo 调整位置和朝向。新方向光/聚光灯默认开启 `Cast Shadow`，点光源关闭；仅场景没有主灯时，新灯自动成为主灯。

选中灯光后，Inspector 的 Type 切换类型，保留对象身份、层级、变换及参数，Cast Shadow 保持原值。三种灯光均支持投影；Shadow bias / Normal bias 以接收位置的阴影纹素为单位。方向光提供 Cascades（1/2/4）、Shadow distance 和 Split lambda。Set as Main Light 指定默认 NPR Key Light 并给予阴影分配优先级，Scene 树以 (main light) 标识；其他灯也能独立投影。删除主灯后不自动指定已有灯，仍可显式指定或创建新灯。参数与主灯选择随场景保存并参与未保存检测。

聚光灯操作：Create Light → Spot，确认 Cast Shadow 开启；在 Render Settings → Shadows 开启 Multiple shadow lights 后，非主灯也能投影。通过 Rotation 将局部 -Z 对准模型，调整 Range 和 Inner Cone / Outer Cone（半角）。点光源需手动开启 Cast Shadow；旋转点光源不改变六面全向投影。方向光选择 2/4 Cascades，再调整 Shadow distance 和 Split lambda。

完整阴影验收建议：

1. 从新项目或现有场景开始，打开 View → Render Settings → Shadows，开启 Multiple shadow lights，预算设为 4，二维分辨率 1024、点光每面 512。旧场景默认保留单主灯模式，需要显式开启多灯。
2. 在地面与模型之间创建两盏不同位置的 Point/Spot，逐灯开启 Cast Shadow；移动各灯、切换主灯、单独关闭一灯的投影，检查另一灯仍独立照明和投影。点光源四周都放遮挡物，沿六面边界观察接缝；旋转点光源不应改变投影。
3. 创建 Directional，设 4 Cascades、Shadow distance 50、Split lambda 0.5。在有近远层次的场景移动/旋转相机、切换场景相机和透视/正交视图；再对比 1/2/4 级。开启 Cascade colors (debug) 检查分级与交界，关闭后检查近处清晰度、远处淡出、阴影抖动和视野外遮挡物的投影。
4. 开启 Shadow map preview，通过 Preview light slot、Face / cascade 选择点光六面或方向光各级；开启 Reflection cascades 检查独立反射级联。预览白色表示远深度/空白，不代表该级一定有可见遮挡物。镜面地面可用 `MaiX_Renderer.exe --material-lab` 启动检查，透明接收可用 `--translucency-test` 场景对照；Masked 孔洞的投影必须与表面一致，细分位移和实例路径也需对照。
5. 增加第 5 盏启用阴影的灯，检查 Shadows 面板显示未分配状态，额外灯仍照明。隐藏/恢复父组、删除正在投影的灯、改变分辨率和视口大小、按 F6 重载 Shader，检查无残留阴影或黑帧。Bias 用于减轻自阴影，过大会出现阴影离开表面的悬浮，应结合实际资产调节。
6. 保存并重开，检查多灯开关、预算、分辨率和每灯参数恢复；修改后切换场景应出现未保存提示。另打开原 v1/v2 场景确认默认保留旧单主灯/单级行为，失败加载不能改变当前场景。已验收的 HDR/IBL、SSAO、FXAA 和既有材质作为对照检查。

本轮多灯、点光源阴影与 CSM 的画面和交互待用户验收。阴影每帧更新；点光每灯六面，CSM 在反射开启时会增加独立级联绘制。层数与分辨率会增加显存和 GPU 成本，当前不承诺固定帧率或性能改善。没有透明彩色投影、点光软光源模型或跨帧阴影缓存。

Inspector 的 `Visible` 控制对象自己的显示开关；父级隐藏会作用于整棵子树，Scene 树用 `(hidden)` 标识有效隐藏对象，`Hidden by parent` 表示开关虽开启但被祖先隐藏。恢复父级不会覆盖子对象自己的开关。隐藏模型不进入主视图、反射或阴影，隐藏灯光不参与照明与阴影；隐藏对象不显示辅助图标/Gizmo，也不参与视口拾取，仍可在 Scene 树选中并通过 Inspector 编辑。相机隐藏只影响编辑图标，其取景仍可使用。

隐藏不会卸载模型或材质，也不会取消资源引用保护。隐藏期间的变换继续由 World 同步，恢复后保留最新位置。EngineSession 的 `PrepareFrame` 统一完成注册、状态同步与多视图帧准备，失效 Actor/组件绑定会拒绝该帧输出。

父级修改会同步子模型、灯光、相机、阴影、包围体和拾取。当前支持能表示为 TRS 的层级变换；非均匀缩放配合旋转若产生剪切，或输入非有限/奇异变换，操作会完整拒绝并显示原因。`Delete Subtree (N)` 删除选中对象及全部后代，并清理渲染引用；删除活动场景相机后返回编辑器视图。

Material Editor 默认跟随所选模型，显示所属模型名称和材质列表。`All materials (debug)` 可查看全部场景材质；点击 Inspector 中的材质会返回模型范围。删除正在编辑的模型后会清理材质选择。

基础材质可通过 `Create Instance` 创建并绑定到当前模型分段。实例支持参数 Override、纹理继承/替换/禁用、重置覆盖和 `Edit Parent`；`Use Parent Material` 恢复基础材质。使用 `--material-instance-lab` 启动父材质与两个实例的共享 Mesh 验收场景。


`Surface Side` 在基础材质中选择，实例通过 `Edit Parent` 修改并随父材质更新：Front 只绘制正面，Back 只绘制背面并翻转光照法线，Double 绘制两面且背面翻转完整法线，Legacy 保留旧版两面可见但不翻转法线的行为。负缩放和镜面反射会修正逻辑正反面；同一材质的正负缩放实例按绕序分批。阴影使用同一面规则，Double 薄片两面都能投影；Masked 的孔洞继续一致裁剪。Front/Legacy 支持已有 Inverted Hull 描边，Back/Double 暂停描边并在面板提示；位移仍沿原始顶点法线。

材质面模式验收：导入一张带 UV/法线贴图的薄片，在 Material Editor 依次切换 Front、Back、Double，绕到背面检查显隐与受光；把模型 X 缩放设为负值，再检查一次。开启灯光 Cast Shadow，对照地面阴影和 Masked 孔洞；在镜面场景检查反射。创建实例后通过 Edit Parent 改面模式，确认两者同步，保存并重开核对模式。旧场景应显示 Legacy，切换后再保存为 v4。材质面模式与双面光照的视觉、交互已由用户验收。


`File → Asset Import` 打开导入面板，先选择 `Normal Y`：默认 `OpenGL (+Y)` 保留原图绿色通道，`DirectX (-Y, flip green)` 在 CPU 处理时翻转绿色，再点击 `Import FBX...`。File 菜单导入和 Content Browser 双击使用当前选项；已导入对象不随选项变化而重处理。FBX 用于图片寻址的 V 翻转保留作者的切线法线基方向；Normal 使用 Linear，Base Color 使用 sRGB；同一图片用于颜色与数据时创建对应 GPU 格式。Material Editor 的纹理区显示 `Sampling`、Normal 转换，以及 `Generated ORM` 的输入文件/内嵌身份、通道和输出尺寸。

AO、Roughness、Metallic 支持明确直连的灰度输入，取 R 通道并合成为 ORM；缺失通道为 1，材质数值继续相乘，不把倍率重复烘焙进图。合成使用最大输入宽高和像素中心双线性重采样，单轴最多 16384、RGBA 输出最多 256 MiB。配方记录每个原图的项目/引擎/外部来源或 FBX 内嵌身份，保存重开时重新生成，不向用户 Assets 写入合成 PNG。实例沿用继承、替换、禁用和重置。

导入范围限于库能识别的单 UV、Repeat、无逐图 UV 变换的明确文件输入；彩色标量图、不同 UV 集、非默认包裹模式、复杂/分层/程序节点、Glossiness 转 Roughness 及 Bump 转 Normal 等会显示警告并回退到常量。已有打包 ORM 可在材质编辑器手工选择，不按文件名猜测通道。新导入的缺失可选贴图会诊断；保存场景的依赖缺失时加载失败并保留旧场景。视觉效果与新导入交互待用户验收。

PBR 导入验收：使用带 Base Color、Normal 和分离灰度 AO/Roughness/Metallic 的 FBX，确认自动绑定与法线凹凸、粗糙/金属区域；分别导入 +Y/-Y 法线，检查方向。修改材质倍率，测试实例继承与替换；检查负缩放、Double 背面和反射。保存重开以及搬移项目后应保持纹理、配方和画面；移走一个必需输入后重开，应报错且当前场景保留。

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
