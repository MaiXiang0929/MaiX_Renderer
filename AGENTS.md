# MaiX Renderer：现代实时光栅化渲染器路线图

## 1. 项目定位

MaiX Renderer 是一个基于 C++17 与 OpenGL 4.0 Core Profile 的轻量级实时光栅化渲染框架。项目目标不是只完成一个展示 Demo，而是逐步建立可解释、可扩展、适合技术美术工作流的渲染架构，重点覆盖：

- GPU 光栅化流程与资源管理
- Cook-Torrance PBR 与 GLSL Shader 开发
- 多 Pass Render Pipeline
- 材质参数和纹理工作流
- NPR/Anime 渲染扩展
- RenderDoc 性能分析

目标岗位方向：渲染技术美术、Shader TA、引擎技术美术。

## 2. 总体架构

```text
Application
  ├─ Window / Input / Camera
  ├─ 游戏侧场景状态
  └─ 每帧提交 RenderFrameData
           │
           ▼
Renderer（渲染资源所有者与管线入口）
  ├─ RenderScene（渲染侧场景代理）
  │   ├─ PrimitiveSceneProxy
  │   └─ LightSceneProxy
  ├─ GPU 资源
  │   ├─ Mesh / Material / Shader / Texture
  │   ├─ Framebuffer
  │   └─ ShadowMap / Cubemap
  └─ RenderPipeline
      ├─ ShadowPass
      ├─ ReflectionPass
      ├─ ForwardPass
      └─ PresentPass
           │
           ▼
        OpenGL GPU
```

### 2.1 当前目录结构

```text
src/
├─ Assets/               FBX 模型与图片导入
├─ Core/                 应用生命周期、窗口、输入、相机
├─ Editor/               灯光调试 Gizmo 等编辑器叠加层
├─ Platform/             平台文件对话框等边界
├─ Renderer/
│  ├─ Core/              Renderer 门面与 GPU 资源所有权
│  ├─ Pipeline/          RenderPipeline、RenderPass 契约、RenderSettings
│  ├─ Passes/            Shadow、Reflection、Forward、Bloom、PostProcess、Present
│  ├─ Resources/         Mesh、Material、Shader、Texture、Framebuffer
│  ├─ Scene/             RenderScene 与 Scene Proxy
│  └─ View/              RenderView 与 RenderItem
└─ main.cpp

assets/shaders/
├─ pbr/                  标准 PBR 与曲面细分/位移 Shader
├─ shadow/               阴影深度 Shader
├─ environment/         Skybox Shader
├─ npr/                  Outline 等 NPR Shader
├─ postprocess/          SSAO、Bloom 与色调处理 Shader
├─ reflection/          反射地面 Shader
├─ present/              离屏结果显示 Shader
└─ debug/                灯光 Gizmo 等调试 Shader
```

## 3. 每帧渲染数据流

```text
逻辑场景对象
    │ Application 更新 Transform / Light
    ▼
RenderScene
    │ BuildRenderView()
    ▼
RenderView
    ├─ opaqueItems
    ├─ translucentItems
    └─ lights
    │
    ├─ ShadowPass：写入 shadow map
    ├─ ReflectionPass：生成反射纹理
    ├─ Forward / Outline / Translucency：写入 HDR Scene Color
    ├─ SSAO / Bloom / PostProcess：屏幕空间遮蔽与色调处理
    └─ EditorPrimitive / Present：合成编辑器叠加层并显示到窗口
```

CPU 负责场景代理、可见项列表、矩阵和资源绑定准备；GPU 负责顶点变换、三角形光栅化、深度测试、纹理采样、BRDF 光照和最终像素输出。当前每个 `RenderItem` 在 Pass 内独立计算 `model`、`MV`、`MVP` 与 `lightMVP`，为后续裁剪、排序和批处理保留扩展点。

## 4. 当前进度（2026-08）

### 已完成

- [x] Renderer 目录按 Core / Pipeline / Passes / Resources / Scene / View 拆分
- [x] Shader 目录按渲染用途拆分
- [x] `RenderPipeline` 按固定顺序执行 Shadow、Reflection、Forward、Outline、Translucency、SSAO、Bloom、PostProcess、EditorPrimitive、Present
- [x] `RenderScene`、`PrimitiveSceneProxy`、`LightSceneProxy` 已建立
- [x] `RenderView`、`RenderItem` 已建立，支持 opaque/translucent 分类
- [x] Renderer 持有 Primitive 的 Mesh 与 Material，Scene Proxy 使用非 owning 指针
- [x] Primitive Proxy 已具备局部空间包围球，并能根据 `localToWorld` 计算保守的世界空间包围球
- [x] `BuildRenderView` 已对主视图、反射视图和阴影视图执行包围球视锥体裁剪，并记录源对象、可见对象和被裁剪对象数量
- [x] 视锥平面提取、球体相交和变换后包围球已有独立 `FrustumTests`，并在 `docs/frustum-culling.md` 记录数据流与限制
- [x] Primitive/RenderItem 已具备稳定 Shader、Material、Mesh 排序 ID；主/反射视图按 Shader / Material / Mesh 排序，阴影视图按 Shader / Mesh / Material 排序
- [x] 不透明排序已有多 Primitive 的 `RenderSceneTests`，主视图运行时输出 Draw 与资源分组统计，排序策略记录于 `docs/opaque-render-sorting.md`
- [x] Forward fragment shader 从 Blinn-Phong 升级为 Cook-Torrance BRDF
- [x] PBR metallic / roughness / AO 参数与 ORM 纹理接入 Material，ORM 使用 R=AO/G=Roughness/B=Metallic
- [x] Base Color、Normal、ORM、Displacement 具备固定槽位与 sRGB/Linear 校验；Legacy Specular 保留兼容回退
- [x] `--material-lab` 提供铜、塑料、陶瓷、粗糙金属四种共享 Mesh 测试材质，数据流记录于 `docs/pbr-material-workflow.md`
- [x] 标准 PBR 与曲面细分/位移路径可切换
- [x] Directional/Spot 阴影基础流程与 PCF
- [x] Cubemap、反射地面、离屏 Framebuffer、Present 流程
- [x] Light Gizmo 由 `EditorPrimitivePass` 写入独立全分辨率 Overlay Buffer，使用预乘 Alpha 并在 Present 合成，不受 Bloom、曝光或 Tone Mapping 影响
- [x] 独立 `TranslucencyPass` 已支持主视图与反射视图透明物体从后向前稳定排序、纹理 Alpha、显式 Blend Mode、Alpha Blend 和深度只读
- [x] PBR 使用 std140 UBO 消费最多 16 盏 Directional/Point/Spot 灯光，并为单一 2D shadow map 记录对应灯光索引
- [x] Renderer 已提供强类型 Mesh/Material Handle 与共享资源提交接口；透明测试场景的三张平面共享一份 Mesh
- [x] `--instance-grid N` 已提供默认关闭的共享资源多实例基准；1/64/256 实例运行数据与 RenderDoc 捕获记录于 `docs/instance-benchmark.md`
- [x] 不透明标准三角形已按 Shader/Material/Mesh 批次执行 Instancing；256 实例时 Shadow/Reflection/Forward 分别降为 1/2/3 Draw，单实例与 Tessellation 保持原路径
- [x] RenderPipeline 已加入可选 GPU Debug Group，各 Pass 具备 Draw 与 Shader/Material/Mesh/Texture 提交统计
- [x] 基于 `TranslucencyPass mesh=3/1` 基线实现最小 VAO 状态缓存，RenderDoc 捕获流程和数据记录于 `docs/renderdoc-baseline.md`
- [x] 三帧缓冲的 GPU Timer Query 已输出各 Pass last/EMA 时间，并以非阻塞方式处理尚未完成的 Query
- [x] ImGui `Renderer Statistics` 面板已显示场景、资源、CPU/GPU Pass 统计，并提供常用渲染调试参数
- [x] Forward 与 Reflection 离屏目标已随窗口 framebuffer 动态重建；反射保持半分辨率，最小化时跳过零尺寸渲染
- [x] Forward Scene Color 已升级为 RGBA16F，PostProcessPass 支持手动曝光、ACES Tone Mapping 与 sRGB 输出编码
- [x] BloomPass 已完成半分辨率 HDR 高亮提取与双向模糊；PostProcessPass 负责 Bloom 合成、曝光、ACES 与 sRGB 编码，PresentPass 仅负责最终显示
- [x] Toon Shading Model 已接入 Forward PBR Shader，支持分段漫反射、阴影色和 Rim Light，并由 Material Editor 实时调节
- [x] 独立 `OutlinePass` 已使用 Inverted Hull 写入 Forward HDR Scene Color；仅处理主视图不透明 Toon 材质，并复用 Forward 深度
- [x] Material Editor 已支持材质选择、PBR/Toon 切换、Toon Outline 参数和 Renderer-owned Material 更新边界
- [x] Outline Pass 已接入 CPU/GPU 提交统计、GPU Timer Query、Shader Reload 和独立 NPR Shader 资源
- [x] SSAO 已使用 Forward 可采样深度重建观察空间位置与法线，完成半分辨率 R8 遮蔽/滤波与全分辨率 HDR 合成
- [x] FBX 静态模型导入已接通多 Mesh、多材质分段、索引绘制、Base Color 纹理、异步 CPU 解析与事务式资源替换
- [x] 编辑器视口已接入相机 Orbit/Pan/Dolly、模型/灯光互斥拾取、ImGuizmo Transform 与选中对象聚焦
- [x] CMake 构建时清理并复制最新 assets
- [x] CMake 配置、编译、链接与 13 项测试通过；Material Lab、默认/Instancing/Tessellation 路径及全部 Shader 初始化检查完成

### 部分完成

- [ ] RenderPipeline 已具备 Pass 边界，但资源依赖仍主要通过共享 Frame Context 传递
- [ ] HDR Scene Color、Bloom、SSAO、手动曝光与色调映射已完成；自动曝光尚未实现
- [ ] 视锥体裁剪已完成包围球粗裁剪，但遮挡裁剪、距离裁剪和更精确的包围体尚未实现

### 尚未开始

- [ ] 共享 Mesh/Material、VAO 状态缓存与不透明 Instancing 已完成，但 Shader/Texture 缓存和跨材质批处理尚未实现
- [ ] HDR Scene Color、Bloom、SSAO、手动曝光与 Tone Mapping 已完成；自动曝光尚未实现
- [ ] Cascaded Shadow Maps（CSM）
- [ ] NPR Anime Shader：Toon、Rim Light、Outline 已完成；Face Shadow、Hair Highlight 尚未实现
- [ ] ImGui 统计面板、Material Editor、Asset Import 和视口 Transform 已完成；Scene Window、Inspector 尚未开始
- [ ] RenderDoc 性能采集和 GPU Pass 统计文档

## 5. 分阶段实施计划

### 阶段一：材质与 PBR 基础（已完成）

目标：完成可复用 Material 工作流和可信的 Cook-Torrance 光照。

1. 统一 `MaterialProperties` 与纹理槽位。
2. 完善 base color、metallic、roughness、AO、normal map 的 Shader 输入。
3. 保持标准网格与曲面细分网格共用 PBR 光照模型。
4. 增加典型金属、塑料、陶瓷、粗糙表面测试材质。
5. 记录坐标空间、BRDF 数学和性能开销。

验收：材质参数改变会实时影响外观，法线/位移/曲面细分不回归。

### 阶段二：RenderScene 与可见性（已完成最小路径）

目标：让渲染器从“固定单物体 Demo”进入多物体场景阶段。

1. 为 Primitive Proxy 补充世界空间包围球/包围盒。
2. 在 `BuildRenderView` 中加入视锥体裁剪。
3. 增加不透明物体排序，减少材质和 Shader 切换。
4. 设计资源句柄或稳定 ID，逐步减少裸指针边界。
5. 增加多物体和多灯光验证场景。

验收：不可见物体不提交 Draw Call，物体数量增加时管线仍可维护。

### 阶段三：阴影与后处理（部分完成）

目标：形成现代实时渲染中常见的 HDR 场景链路。

1. 将 ShadowMap 资源和 ShadowPass 生命周期显式化。
2. 实现 CSM，处理级联划分、稳定投影和级联混合。
3. 建立 HDR color target、亮度提取和 Bloom pass。
4. 增加曝光与 Tone Mapping。
5. 增加 SSAO，并定义其与主光照的组合方式。

验收：明暗范围稳定，Bloom/SSAO 可独立开关，RenderDoc 能看到清晰 Pass 边界。

### 阶段四：透明与材质表现扩展（当前阶段）

目标：补齐透明渲染并为艺术表现提供可切换路径。

1. 实现 `TranslucencyPass` 和透明物体深度/排序策略。
2. 增加 PBR 材质实例和参数覆盖。
3. 增加 NPR 管线入口。
4. 实现 Toon Lighting、Face Shadow、Outline、Rim Light、Hair Highlight。
5. 允许 PBR / NPR 按材质或视图模式切换。

验收：透明物体顺序正确，二次元材质参数可由工具实时修改。

### 阶段五：编辑器与性能分析（部分完成）

目标：形成技术美术可操作、可观察的工作流。

1. 集成 ImGui。
2. 实现 Scene Window、Inspector、Material Editor。
3. 暴露 Transform、Light、Base Color、Metallic、Roughness、Texture 等参数。
4. 增加 GPU debug label、Pass 时间和 Draw Call 统计。
5. 使用 RenderDoc 记录基线、瓶颈和优化结果。

验收：运行时调整参数立即反映到画面，性能数据可复现并有优化前后对比。

## 6. 近期下一步

下一次实现前仍遵循“先给方案、确认后执行”的协作流程。建议优先顺序为：

1. 为已导入的角色定义 Face Shadow 线性纹理通道、面部局部朝向与左右镜像契约，实现只由主光驱动的最小 Toon Face Shadow。
2. Face Shadow 达到可展示后，再补充最小 Scene Window / Inspector 的选择、Transform 与灯光参数工作流。
3. 为透明材质补充 Masked/Additive 模式前，先明确 Alpha Cutout 阴影、排序和混合策略。

## 7. 当前技术债与约束

- OpenGL 资源仍由各 Resource 类直接管理，尚未抽象成跨 API RHI。
- Renderer 通过只增资源表和强类型 Handle 拥有 Mesh/Material，Scene Proxy 与 `RenderItem` 仍缓存非 owning 裸指针；资源删除、generation 校验和 Primitive/Light 完整移除边界仍需补充。
- RenderPass 之间仍通过共享 `RenderPassContext` 和 Pass 之间的直接引用传递视图及纹理资源，资源读写依赖尚未显式声明，后续可引入 Render Graph。
- 当前裁剪只使用世界空间包围球，非均匀缩放取最大轴形成保守半径；细长物体可能产生误保留，但不会错误剔除。遮挡裁剪与距离裁剪尚未实现。
- 主视图、反射视图和阴影视图每帧分别遍历场景并重建 `RenderItem` 列表；对象规模扩大后需要评估重复 CPU 遍历、容器填充和包围体变换成本。
- 不透明列表已按稳定资源 ID 排序并按 Shader/Material/Mesh 批次执行 Instancing；每实例通过 64 字节 model-view UBO 输入，OpenGL 4.0 的 16 KiB 可移植上限使单 Draw 最多容纳 256 个实例。透明与 Tessellation 仍使用逐项提交，Texture cache 留待多批次场景重新评估。
- 透明排序使用各自主视图/反射视图下的物体包围球中心观察空间深度；相交网格、网格内部三角形顺序、Alpha Cutout 阴影与 OIT 仍未处理。
- Resize 会在渲染线程立即重建 Forward/Reflection 颜色与深度附件；持续拖动窗口可能产生重复 GPU 分配，后续可结合 Render Graph 资源池或 resize debounce 优化。
- Forward PBR 最多消费 16 盏灯；当前仍只有一张 2D shadow map，Point Light 阴影与多阴影灯尚未实现。
- Outline 使用观察空间厚度，当前只覆盖主视图不透明 Toon 材质；Tessellation 开启时跳过，透明/反射描边、屏幕空间恒定像素宽度和 Outline Instancing 尚未处理。
- SSAO 当前从深度重建观察空间法线，使用固定屏幕空间采样核，并在 HDR Scene Color 上统一合成；间接光分离、时域稳定和 GTAO/HBAO 不在当前边界。
- 透明、HDR、后处理和多灯光会显著扩大 GPU 资源与调试范围，应逐步加入 RenderDoc 基线。


## 8. 最终展示目标

最终 Demo 以 60 秒展示完整工作流：

```text
0-10s   MaiX Renderer 与架构概览
10-25s  PBR：金属、塑料、粗糙材质
25-40s  NPR：Toon、Outline、Face Shadow
40-50s  HDR、Bloom、SSAO、Tone Mapping
50-60s  Editor 中实时修改材质和灯光参数
```

项目最终应能清楚展示：C++、OpenGL、GLSL、PBR、NPR、Render Pipeline、RenderDoc 与技术美术工具开发能力。

## 9. 下一步方案对齐门禁

每次提出或修改“下一步执行方案”前，必须重新检查第 8 节的 60 秒最终展示目标，不能因为某个已有模块仍可继续完善，就默认深化该模块。

规划前必须回答：

1. 本步骤对应最终展示的哪个时间段？
2. 它补齐哪个尚未展示的核心能力？
3. 如果不做，是否会阻塞最终 Demo？
4. 本轮最小可展示实现是什么？
5. 哪些扩展明确留到以后？

优先级依次为：

1. 补齐最终 Demo 中尚未出现的能力。
2. 修复阻塞展示的正确性、稳定性或工作流问题。
3. 完成连接已有能力所必需的最小架构调整。
4. 仅在基准或 RenderDoc 证据表明存在瓶颈时进行性能优化。

以下事项不得自动成为近期优先项：

- 没有基准数据支持的缓存、批处理或底层优化。
- 不影响最终展示画面的通用化和跨 API 抽象。
- 暂无实际使用者的扩展接口或资源系统。
- 测试场景、材质变体和调试统计的无限扩充。
- 与 60 秒展示内容没有直接关系的小功能完整性完善。

每份下一步执行方案必须包含：

- **展示目标映射**：对应第 8 节的具体时间段和画面。
- **当前关键缺口**：说明为什么此项比其他未完成项更优先。
- **本轮最小交付**：只包含形成可运行、可观察结果所需的工作。
- **验收方式**：定义画面、交互、测试或 RenderDoc 证据。
- **明确不做**：列出本轮不会顺带扩展的内容。

单个子系统达到“可展示、可解释、无明显正确性错误”后即停止扩展，重新回到第 8 节选择下一个关键缺口。

## 10. 执行后验证约定

每轮方案执行完成后更新本文档内的相关内容，助手只负责验证程序能够正常运行，包括构建、启动以及明显的初始化、Shader 编译或链接错误。

视觉效果由用户验收。助手应提供启动参数、操作步骤和画面检查项，并明确标记视觉效果为“待用户验收”，不得声称未实际观察到的视觉结果已经通过。
