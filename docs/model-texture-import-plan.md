# MaiX Renderer 模型与贴图导入执行方案

状态：已执行，视觉效果待用户验收  
目标资产：`D:\Desktop\Unity URP shader\重逢-荧\Lumine FBX.fbx`  
记录日期：2026-08-17

## 1. 结论

本轮不重新设计资源系统，而是在当前未完成的 `ufbx + stb_image` 导入分支上收口一条可运行链路：

```text
ImGui 选择 FBX
    -> 工作线程解析 FBX、三角化、生成切线、解码被引用贴图
    -> 主线程创建 Texture2D / Material / Mesh
    -> Renderer 创建每个材质分段对应的 Primitive
    -> RenderScene 进入既有 Shadow / Forward / Outline / Translucency 流程
```

最小交付是让目标 FBX 以静态姿态导入，保留多 Mesh、多材质分段和索引，自动解析并绑定 FBX 实际引用的 Base Color 贴图，同时允许在 Material Editor 中从目标 `textures` 目录手动选择 PNG/JPG。Lightmap、Shadow Ramp、Face Shadow、Matcap 等二次元专用贴图本轮只作为候选资产被发现，不提前扩张 Shader 语义。

## 2. 最终展示目标映射

### 2.1 对应时间段和画面

- **25-40s NPR**：为 Toon、Outline、后续 Face Shadow 提供真正的角色模型、多材质和面部/头发贴图资产。
- **50-60s Editor**：展示从编辑器导入 FBX、选择材质、查看或替换贴图，并立即反映到场景。

### 2.2 当前关键缺口

当前启动场景仍由 `Application` 直接读取固定 OBJ，并将其压平成一份 Mesh 和一份主要 Material。虽然工作区已有 FBX、图片解码、文件对话框和资源销毁等未完成代码，但还没有接通 `Application -> Renderer -> RenderScene` 的完整提交路径，主程序接口目前也不一致。

目标角色资产是后续 Face Shadow 展示的前置条件。继续完善缓存、批处理或通用 RHI 都不能补齐角色画面，因此模型与贴图导入优先级更高。

### 2.3 不做是否阻塞最终 Demo

会阻塞。没有角色模型、多材质分段和贴图导入，25-40s 只能继续展示茶壶上的 Toon/Outline，无法形成角色 Face Shadow 与 Hair Highlight 的可信展示；50-60s 也缺少可演示的资产工作流。

### 2.4 本轮最小可展示实现

1. 从编辑器选择目标 FBX。
2. 后台完成 CPU 解析，主线程完成 GPU 资源创建。
3. 51 个材质分段以独立 Primitive 进入现有 RenderScene。
4. FBX 引用的 11 张 Base Color 贴图自动绑定，材质名称可在 Material Editor 中辨认。
5. 其余 PNG/JPG 可由 Material Editor 手动选择、预览、清除。
6. 重复导入时旧模型资源正确释放，失败时保留原场景并显示错误。

### 2.5 明确留到以后

- 骨骼、蒙皮、动画播放与动画状态机。
- Face Shadow、Lightmap、Shadow Ramp、Matcap、Hair Highlight 的 Shader 采样和材质槽。
- Masked、Alpha Cutout、Additive 及透明阴影。
- glTF、OBJ 新导入器、拖拽导入、资源数据库、缩略图缓存和热重载。
- Scene Window、完整层级树、节点级 Inspector 和通用导入预设。
- Texture Streaming、压缩纹理、异步 GPU 上传和跨导入全局缓存。

## 3. 目标资产审计结果

### 3.1 FBX 实测

现有 `FbxModelImporterTests` 已直接读取目标文件，结果如下：

| 项目 | 实测值 |
|---|---:|
| FBX 文件大小 | 24,597,820 bytes |
| 源 Mesh | 29 |
| 可渲染材质分段 | 51 |
| 源 Material | 33 |
| FBX 引用且当前可解码的贴图 | 11 |
| Skin Deformer | 19 |
| Animation Stack | 0 |
| 总包围盒最小值 | `(-0.474, -28.205, -7.774)` |
| 总包围盒最大值 | `(100.587, 1.561, 8.450)` |

当前导入器会计算已评估的蒙皮顶点，并把结果烘焙为静态几何，不保留运行时骨骼数据。目标 FBX 没有动画栈，因此这满足本轮静态展示边界。

总包围盒不能直接用于自动取景。以下 4 个分段位于约 `X=99m, Y=-27m`，属于与主体相距很远的武器/弹匣对象：

- `IWS mag bullets Electro.001:Electro Tape`
- `IWS mag bullets Electro.001:IWS_Secondary`
- `IWS mag bullets Dendro.001:IWS_Secondary`
- `IWS mag bullets Dendro.001:Dendro Tape`

本轮仍导入这些合法几何，但不使用整份 FBX 的总包围盒强制重设相机。节点筛选和 Scene Window 留到后续。

### 3.2 贴图实测

目标 `textures` 目录共有 30 张可解码图片，格式为 PNG/JPG，主要尺寸为 2048x2048，包含：

- Base Color / Diffuse：角色面部、头发、服装、道具和武器。
- NPR 数据：Body/Hair/Outfit Lightmap、Body/Hair Shadow Ramp、Face Lightmap、Face Shadow。
- 表现扩展：MetalMap、Matcap、Light、Outline、Decal。

FBX 内实际声明的贴图主要是 11 张 Base Color/Diffuse；目录中其余贴图没有可靠的 FBX 材质语义。不能仅凭文件名把它们猜成 ORM、Normal 或其他现有槽，否则会产生错误的通道解释和颜色空间。

颜色空间规则：

| 数据类型 | 颜色空间 | 本轮处理 |
|---|---|---|
| Base Color / Diffuse / 颜色型 Decal | sRGB | FBX 引用的 Base Color 自动创建并绑定 |
| Normal / ORM / Lightmap / Ramp / Face Shadow / MetalMap | Linear | 仅在明确槽位或后续 Shader 契约中使用 |
| Matcap | 由后续 Shader 契约明确 | 本轮不自动绑定 |

未被材质使用的候选图片不在导入时全部解码和上传，避免一次导入产生数百 MB 的无效 RGBA8 CPU/GPU 数据。

## 4. 当前架构判断

### 4.1 可复用部分

- `Assets/Import/FbxModelImporter`：使用 `ufbx`，已覆盖 Unicode 路径、轴和米制单位转换、三角化、多材质分段、索引生成、UV、法线、切线、包围球、外部/内嵌 Base Color。
- `Assets/Import/ImageLoader`：使用 `stb_image`，将 PNG/JPG 解码为统一 RGBA8 CPU 数据。
- `Renderer`：已有强类型 Mesh/Material Handle、共享资源所有权和 Primitive 提交。
- `Mesh`：未完成分支已增加 EBO 和索引绘制路径。
- `Material` / `MaterialEditorPanel`：未完成分支已增加名称、贴图来源、预览、选择和清除。
- `AssetImportPanel` / Windows `FileDialog`：已有异步解析和文件选择雏形。

### 4.2 必须收口的问题

1. `Application.h`、`Application.cpp` 与 `main.cpp` 的构造参数不一致。
2. `Application.h` 已声明导入资源组和提交函数，但 `Application.cpp` 尚未实现，并仍访问已移除的单 Primitive 字段。
3. `AssetImportPanel.cpp` 尚未加入主程序 CMake 源文件列表，也未在 Application 初始化、每帧更新、绘制和关闭阶段接入。
4. CPU 导入结果尚未在持有 OpenGL Context 的主线程转换为 GPU 资源。
5. 重复导入时 Primitive、Mesh、Material 的替换顺序和失败回滚尚未闭合。
6. 目标资产总包围盒包含远离主体的合法分段，不能直接覆盖现有相机、地面和阴影拟合参数。
7. 当前自动材质映射只有 Base Color；这应作为本轮明确边界，而不是伪装成完整 Unity URP 材质还原。

## 5. 设计与责任边界

### 5.1 CPU 侧

`FbxModelImporter` 只生成不依赖 OpenGL 的 `ImportedModelData`：

- 将 FBX 转换到右手、Y-up、米制空间。
- 按 Node 的 Material Part 生成 `ImportedMeshData`。
- 输出 `Position / Normal / TexCoord / Tangent` 和 `uint32` 索引。
- 使用评估后的蒙皮顶点烘焙静态姿态；顶点已经位于目标世界空间，分段 `localToWorld` 保持单位矩阵。
- 输出材质参数、Base Color 贴图引用、每分段包围球和警告。
- 工作线程执行文件读取、FBX 解析和图片解码，不调用 ImGui 或 OpenGL。

### 5.2 GPU 侧

`Application::CommitImportedModel()` 在渲染主线程执行：

1. 每个已导入贴图只创建一次 `Texture2D`，Base Color 使用 `GL_SRGB8_ALPHA8`。
2. 每个导入材质创建一份 Renderer-owned `Material`，同一贴图由 `shared_ptr<Texture2D>` 共享。
3. 每个材质分段创建一份带 EBO 的 Renderer-owned `Mesh`。
4. 使用分段 `materialIndex` 创建 Primitive，并传入局部包围球。
5. 只有全部必要资源和 Primitive 创建成功后，才把新资源组设为 Active Model。
6. 成功切换后按 `Primitive -> Mesh -> Material` 顺序移除旧资源；失败则清理本次部分创建结果并保留旧模型。

Texture 生命周期由 Material 的 `shared_ptr` 持有；Mesh、Material 和 Primitive 生命周期由 `Application::ModelResourceGroup` 记录，实际 GPU 资源仍由 Renderer 所有。

### 5.3 每帧渲染数据流

导入成功后不增加新的 Render Pass：

```text
Imported Mesh/Material
    -> Renderer-owned resources
    -> PrimitiveSceneProxy
    -> BuildRenderView() 裁剪与排序
    -> Shadow / Reflection / Forward / Outline / Translucency
    -> PostProcess / Present
```

GPU 继续负责顶点变换、光栅化、深度、材质采样和光照。导入只改变资源创建与场景提交，不让 Pass 直接感知 FBX 或磁盘路径。

## 6. 分步执行计划

### 步骤 0：保护并收口当前未完成分支

目标：先恢复一致的可编译基线，不覆盖现有改动。

- 逐项对齐 `Application` 构造函数、命令行参数和成员字段。
- 保留已经存在的透明测试开关，不把本轮导入工作与透明功能回退混在一起。
- 将 `AssetImportPanel.cpp` 和相关导入/平台文件正确加入 CMake。
- 确认 `ufbx.c`、`stb_image.h`、Windows 文件对话框依赖和测试目标只定义一次。
- 先构建主程序和现有测试，修复纯集成错误后再继续。

完成条件：主程序、`ImageLoaderTests` 和 `FbxModelImporterTests` 可编译链接。

### 步骤 1：冻结 CPU 导入契约

目标：让目标 FBX 解析结果稳定、可测试、与 OpenGL 解耦。

- 保留多材质分段，不再次合并成单 Mesh/Material。
- 校验所有索引均落在顶点数组范围内，所有包围值为有限数。
- 保留 Unicode 路径和 `textures` 子目录的大小写不敏感回退。
- 对 Blender 风格的重复名称后缀（例如 `name.png.001`）只在精确路径解析失败后做候选文件名回退，并记录 warning。
- 将 30 张目录图片作为候选元数据列出，但只解码 FBX 材质实际引用的图片。
- 在结果中明确记录静态蒙皮烘焙、缺失贴图和远距离分段警告。

完成条件：使用 `MAIX_TEST_FBX_PATH` 指向目标 FBX 时，稳定得到 29 个源 Mesh、51 个分段、33 个材质和有效边界，且无索引越界。

### 步骤 2：完成主线程 GPU 提交和资源替换

目标：把 CPU 导入结果安全地送入现有渲染管线。

- 实现索引 Mesh 上传与 Draw/DrawInstanced/DrawPatches 的兼容路径。
- 将导入贴图按语义创建为 sRGB 或 Linear 资源。
- 创建有名称的 Material，并自动绑定 Base Color。
- 创建每个分段的 Primitive，使用已有 PBR Shader 作为默认可见路径；用户可在 Material Editor 切换为 Toon。
- 实现事务式 Active Model 切换和部分失败清理。
- 不用目标资产的全局 100m 包围盒自动重设相机、地面或阴影参数；本轮保留已有相机交互，由用户旋转/缩放取景。

完成条件：导入成功后原启动模型被替换，导入失败或再次选择取消时原模型不消失，资源统计不会在重复导入后持续无界增长。

### 步骤 3：完成编辑器工作流

目标：形成可操作、可观察的最小技术美术入口。

- `File -> Import FBX...` 和 Asset Import 面板共用同一导入状态。
- 面板显示 Parsing、Uploading、Ready、Error，不在 UI 线程等待 CPU 解析。
- Ready 状态显示源路径、分段数、材质数、已绑定贴图数和 warning。
- Material Editor 使用真实材质名称列出 33 个材质。
- 每个现有纹理槽显示来源与缩略图，并支持选择 PNG/JPG 或清除。
- 文件对话框取消不算错误；解析或上传错误保留可读消息。

完成条件：用户能导入目标 FBX、定位一个角色材质、看到 Base Color 已绑定，并能手动替换后立即观察材质变化。

### 步骤 4：验证与记录

目标：只验证运行稳定和明显初始化错误；视觉效果由用户验收。

- 运行 CMake 配置、编译和全部 CTest。
- 运行目标 FBX 的可选集成测试。
- 启动主程序，检查窗口、OpenGL 初始化、Shader 编译/链接和导入错误日志。
- 执行两次连续导入，检查旧 Primitive 和资源清理。
- 记录目标资产的最终分段、材质、贴图和 warning 数量。
- 给出用户操作步骤和画面检查项，并标记为“视觉效果待用户验收”。

## 7. 预计修改范围

| 文件/模块 | 本轮职责 |
|---|---|
| `CMakeLists.txt` | 接入导入器、Asset Import Panel、ufbx、stb_image 和测试 |
| `src/Assets/Import/*` | CPU 数据契约、FBX 解析、图片解码和路径解析 |
| `src/Platform/Windows/FileDialog.*` | FBX 与 PNG/JPG 文件选择 |
| `src/Editor/AssetImportPanel.*` | 异步状态、导入入口、结果与 warning 展示 |
| `src/Editor/MaterialEditorPanel.*` | 材质名称、贴图预览、选择和清除 |
| `src/Core/Application.*` | GPU Commit、Active Model 生命周期和 UI 接线 |
| `src/Renderer/Resources/Mesh.*` | EBO 上传和索引 Draw |
| `src/Renderer/Resources/Material.*` | 名称、贴图来源和共享 Texture 所有权 |
| `src/Renderer/Core/Renderer.*` | 资源创建/销毁、材质快照和 Primitive 移除门面 |
| `tests/Assets/Import/*` | 图片、错误路径、Unicode 路径和目标 FBX 集成验证 |

不修改 RenderPipeline 的 Pass 顺序，也不在 Pass 中加入 FBX、文件系统或编辑器依赖。

## 8. 验收方式

### 8.1 自动验证

1. CMake 配置、主程序编译和链接成功。
2. 现有测试全部通过。
3. `ImageLoaderTests` 覆盖内存 PNG、无效数据、缺失路径和 Unicode 路径。
4. `FbxModelImporterTests` 覆盖缺失/无效 FBX，并通过环境变量运行目标资产集成检查。
5. 主程序启动后没有明显 OpenGL 初始化、Shader 编译或链接错误。

### 8.2 验收准备

使用以下可执行文件：

```text
D:\Code\Projects\OpenGL_Project\out\build\windows-ninja-debug\OpenGL_Project.exe
```

目标模型：

```text
D:\Desktop\Unity URP shader\重逢-荧\Lumine FBX.fbx
```

验收时建议保留以下三个面板可见：

- `Asset Import`：检查导入状态、数量和 warning。
- `Material Editor`：检查材质名称、贴图预览和材质切换。
- `Renderer Statistics`：检查场景 Primitive 和资源数量。

相机操作：

- 左键拖动：旋转主场景相机。
- 右键拖动：缩放主场景相机。
- 验收角色时不要按住 `Alt`；`Alt` 会操作 Present 平面相机。

### 8.3 分步验收清单

#### A. 启动与初始状态

- [ ] 启动后出现 `OpenGL Engine` 窗口，没有立即退出或黑屏后崩溃。
- [ ] 初始茶壶场景可以显示，`Asset Import` 面板状态为 `Idle`。
- [ ] `File` 菜单中存在 `Import FBX...`，按钮可点击。
- [ ] 拖动窗口或操作相机时程序保持响应。

通过标准：程序正常运行，Shader 编译/链接失败信息没有出现在控制台。

#### B. 第一次导入目标 FBX

1. 点击 `File -> Import FBX...`。
2. 选择 `D:\Desktop\Unity URP shader\重逢-荧\Lumine FBX.fbx`。
3. 观察 `Asset Import` 状态变化。

- [ ] 状态先进入 `Parsing`，完成后进入 `Uploading`，最终变为 `Ready`。
- [ ] 解析期间窗口仍可响应，不会长时间显示“未响应”。
- [ ] Ready 状态显示 `51 sections`。
- [ ] Ready 状态显示 `33 materials`。
- [ ] Ready 状态显示 `11 textures`。
- [ ] Ready 状态显示 `30 texture candidates discovered`。
- [ ] Warnings 中包含静态蒙皮烘焙提示。
- [ ] Warnings 中包含 4 个远距离 Mesh 分段提示。
- [ ] 导入完成后原茶壶被角色模型替换，而不是与角色重复存在。

允许短暂卡顿：`Uploading` 在主线程创建 51 份 Mesh 和 11 张 GPU 贴图，可能出现一次性上传停顿；持续卡死或进程退出视为失败。

#### C. 模型几何与相机

使用左键旋转、右键缩放，把原点附近的角色主体放到画面中央，然后检查：

- [ ] 角色整体朝向合理，没有躺倒、镜像或上下颠倒。
- [ ] 身体比例正常，没有明显的单位换算错误或某个轴被拉伸。
- [ ] 面部、头发、服装和身体主要分段都存在，没有整块几何缺失。
- [ ] 三角形连接正常，没有大面积飞面或顶点被拉到错误位置。
- [ ] 正面光照下法线方向合理，没有大面积表面完全发黑。
- [ ] 背面剔除没有让本应可见的外表面消失。
- [ ] 贴图 UV 没有整体上下翻转、左右镜像或明显错位。
- [ ] 导入时相机没有因为约 100m 外的武器/弹匣分段被自动拉到极远处。

已知的 4 个远距离分段仍会被导入。它们离角色主体很远，本轮没有 Scene Window 用于选择或删除；这不算导入失败。

#### D. Base Color 与材质分段

在 `Material Editor` 中依次选择若干面部、头发、服装和道具材质：

- [ ] Material 下拉列表显示真实材质名称，而不是只有数字编号。
- [ ] 同名材质仍可分别选择，选择时不会跳到错误条目。
- [ ] 已引用贴图的材质在 `Base Color` 槽显示缩略图。
- [ ] Base Color 来源显示外部文件路径或 `Embedded: <贴图名>`。
- [ ] 面部材质显示面部贴图，头发材质显示头发贴图，服装材质显示对应服装贴图。
- [ ] 不同材质分段没有全部错误地共用同一张 Base Color。
- [ ] Base Color 的明暗与颜色基本合理，没有明显的重复 Gamma 导致的过暗或过亮。

建议至少检查以下可辨认内容：角色面部、头发、主体服装、角色道具以及一件武器材质。

#### E. 手动替换和清除贴图

选择一个容易辨认的角色材质，在其 `Base Color` 行执行：

1. 点击 `Select...`。
2. 从目标 `textures` 目录选择另一张 Diffuse/Base Color PNG。
3. 观察缩略图、来源名称和场景画面。
4. 点击 `Clear`。

- [ ] 文件选择器支持中文目录和带空格的文件名。
- [ ] 选择后缩略图和来源名称立即更新。
- [ ] 场景中对应材质分段的颜色立即变化，其他材质不受影响。
- [ ] 点击 `Clear` 后该槽变为 `Not bound`，程序不崩溃。
- [ ] 再次导入 FBX 后，原始 Base Color 绑定恢复。

只用颜色贴图验收 `Base Color`。不要把 Lightmap、Shadow Ramp、Face Shadow 或 Matcap 塞进 Normal/ORM 槽来判断正确性；这些贴图还没有对应 Shader 契约。

#### F. PBR、Toon 与 Outline 兼容性

选择一个不透明角色材质：

1. 将 `Shading Model` 从 `PBR` 切换为 `Toon`。
2. 开启 `Outline`。
3. 调整 Toon Threshold、Shadow Strength、Rim Light 和 Outline Thickness。

- [ ] PBR/Toon 切换不会导致材质消失或程序崩溃。
- [ ] Toon 参数变化能在对应材质分段上实时反映。
- [ ] Rim Light 参数变化可观察。
- [ ] 开启 Outline 后，对应不透明 Toon 分段出现描边。
- [ ] 关闭 Outline 后描边消失。

本项只验证导入材质能进入已有 Toon/Outline 路径，不代表 Face Shadow、Ramp 或 Hair Highlight 已完成。

#### G. 重复导入与资源清理

1. 第一次导入完成后，记录 `Renderer Statistics` 中的 Scene、Mesh Resource 和 Material Resource 数量。
2. 再次导入同一 FBX。
3. 等待状态重新变为 `Ready`，再次记录统计。

- [ ] 第二次导入仍得到 `51 sections / 33 materials / 11 textures / 30 candidates`。
- [ ] 场景中只保留一份角色，没有重叠的重复模型。
- [ ] Scene Primitive 数量没有在第二次导入后从约 51 翻倍到约 102。
- [ ] Mesh Resource 数量没有从约 51 翻倍到约 102。
- [ ] Material Resource 数量没有从约 33 翻倍到约 66。
- [ ] 第二次导入后材质仍可编辑，Draw Call 和 Shader 提交没有失效。
- [ ] 连续导入过程中没有访问冲突、OpenGL 错误或崩溃。

Renderer 的 Handle 表当前是只增并保留空槽，但统计面板只计算仍存活的资源；因此 Handle ID 变大不算失败，存活资源数量翻倍才算失败。

#### H. 取消操作

- [ ] 打开 `Import FBX...` 后点击取消，当前场景保持不变。
- [ ] 打开材质贴图 `Select...` 后点击取消，当前贴图保持不变。
- [ ] 取消操作不会把面板状态改成 `Error`。

### 8.4 通过标准

以下条件全部满足即可判定本轮“模型与贴图导入”通过：

1. FBX 能稳定进入 `Ready`，数量为 `51 / 33 / 11 / 30`。
2. 角色主要几何、朝向、法线和 UV 无明显正确性错误。
3. FBX 引用的 Base Color 与主要材质分段对应正确。
4. Material Editor 能选择材质、替换贴图、清除贴图并实时反映。
5. 导入材质能进入已有 PBR、Toon 和 Outline 路径。
6. 重复导入不会留下第二份模型或翻倍存活资源。
7. 取消文件选择不会破坏当前场景。

以下现象属于已知限制，不作为本轮失败条件：

- 角色保持静态姿态，没有骨骼动画。
- Lightmap、Shadow Ramp、Face Shadow、Matcap、MetalMap 和 Hair Highlight 尚未自动绑定或参与 Shader。
- 头发、Decal 等 Alpha Cutout/Masked 表面可能显示不正确。
- 4 个远距离武器/弹匣分段仍存在于场景数据中。
- 没有 Scene Window、节点层级或单独隐藏 Mesh 的功能。

### 8.5 失败时记录

发现问题时记录以下内容，便于定位到 CPU 导入、GPU 资源、材质或渲染阶段：

- `Asset Import` 面板截图，包括状态、数量和完整 warning。
- 角色整体截图，以及问题局部的正面和背面截图。
- `Material Editor` 中出问题材质的名称、Shading Model、Blend Mode、贴图缩略图和来源。
- `Renderer Statistics` 在第一次与第二次导入后的截图。
- 控制台中从点击导入开始到失败后的完整错误文本。
- 问题是否能在重新启动程序后稳定复现。

视觉结果必须由用户确认；本轮执行方只声明程序构建、启动和明显初始化错误检查结果。

## 9. 风险与处理

| 风险 | 本轮处理 |
|---|---|
| 19 个 Skin Deformer 但无动画栈 | 烘焙当前静态姿态并显示 warning |
| 目标资产存在远距离分段 | 仍导入，但不使用总包围盒自动取景；节点筛选后续实现 |
| 贴图文件名含空格、中文路径或 Blender `.001` 后缀 | 使用 `std::filesystem`、UTF-8/Unicode 文件对话框和失败后候选匹配 |
| PNG 有 Alpha，但材质透明语义不明确 | 不按图片 Alpha 自动猜测 Blend Mode；Masked/Additive 后续处理 |
| 专用 NPR 贴图没有现成 Shader 槽 | 仅作为候选资产，不错误塞入 ORM/Normal 等槽 |
| 51 个分段导致较多 Draw Call | 本轮先保证正确导入；只有 RenderDoc 证明瓶颈后才讨论合批 |
| 大贴图解码和上传产生瞬时卡顿 | CPU 解码放工作线程，GPU 上传在主线程一次提交；不上传未使用候选贴图 |
| 资产授权与署名 | Demo 和文档中保留原作者署名，不把第三方资产提交为自有作品 |

## 10. 停止条件与后续入口

当目标 FBX 能稳定导入、Base Color 正确进入现有材质、编辑器能观察和替换贴图，且构建与运行无明显错误时，本子系统即达到“可展示、可解释、无明显正确性错误”，停止继续通用化。

下一轮重新回到 60 秒目标选择缺口，优先为该角色定义 Face Shadow 的面部朝向、Face Shadow/Lightmap 纹理契约和最小 Shader 实现，而不是继续扩展模型格式或资源数据库。

## 11. 执行记录

2026-08-17 已完成本方案的最小实现：

- 接通 `File -> Import FBX...`、后台 CPU 解析和主线程 GPU Commit。
- 接通多材质分段、索引 Mesh、Base Color 自动绑定和 Material Editor 贴图选择。
- 实现 Active Model 的事务式替换和 Primitive/Mesh/Material 清理顺序。
- 目标 FBX 集成测试得到 29 个源 Mesh、51 个分段、33 个材质、11 张已引用贴图、30 张候选贴图、19 个 Skin Deformer、0 个动画栈。
- 正确报告静态蒙皮烘焙和 4 个远距离分段 warning。
- CMake 配置、编译、链接和 11 项 CTest 通过。
- 主程序进程可启动并持续运行，未发生立即退出；由于自动化环境无法访问交互式 Windows 桌面，文件选择器和最终画面标记为待用户验收。
