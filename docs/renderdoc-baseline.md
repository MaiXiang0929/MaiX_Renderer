# RenderDoc 与渲染提交基线

## 环境与捕获

基线采集环境：

- OpenGL 4.0 Core Profile
- NVIDIA GeForce RTX 4060 Ti
- RenderDoc 命令行捕获，启用 API Validation
- 场景：1 个不透明茶壶、3 个共享 Mesh 的透明平面、3 盏灯、反射地面

本地捕获文件为 `captures/maix_baseline_frame2749.rdc`，大小约 78 MiB。`.rdc` 属于机器生成的大文件，由 `captures/.gitignore` 排除，不进入源码版本控制。捕获已通过 `renderdoccmd thumb` 成功解析，缩略图确认茶壶、透明层、反射和灯光 Gizmo 均正常。

## GPU Pass 标记

`RenderPipeline` 使用可选 `GpuDebugScope` 为每个 Pass 写入以下调试组：

```text
MaiX.ShadowPass
MaiX.ReflectionPass
MaiX.ForwardPass
MaiX.TranslucencyPass
MaiX.EditorPrimitivePass
MaiX.PresentPass
```

项目仍以 OpenGL 4.0 为最低版本。`GpuDebugScope` 在运行时查询 `glPushDebugGroup` / `glPopDebugGroup` 及 KHR 后缀版本；驱动不支持时退化为空操作，不改变渲染流程。

## CPU 提交基线

日志格式中的 `请求/变化` 表示 CPU 发起的绑定请求数，以及相邻提交真正需要切换资源的次数。首次测量结果：

| Pass | Draw | Shader | Material | Mesh | Texture |
| --- | ---: | ---: | ---: | ---: | ---: |
| Shadow | 1 | 1/1 | 0/0 | 1/1 | 0/0 |
| Reflection | 2 | 2/2 | 1/1 | 2/2 | 7/7 |
| Forward | 3 | 3/3 | 1/1 | 3/3 | 10/9 |
| Translucency | 3 | 1/1 | 3/3 | 3/1 | 2/2 |
| EditorPrimitive | 3 | 3/3 | 0/0 | 3/3 | 0/0 |
| Present | 1 | 1/1 | 0/0 | 1/1 | 1/1 |

Forward 与 Translucency 都复用同一个 PBR Shader，但它们是独立 Pass，因此缓存边界会在 Pass 开始时失效。Forward 中天空盒、反射地面和不透明表面使用不同 Shader/Mesh，切换是必要的。

## 优化决策

透明 Pass 的 `mesh=3/1` 是当前最明确的冗余：三张透明平面共享同一 VAO，三次 Draw 只有第一次需要绑定。基于该证据实现最小 `OpenGLStateCache`：

- 只缓存当前 VAO。
- 每个 Pass 开始时失效，避免与未经过缓存的 OpenGL 调用产生错误假设。
- `Mesh::Draw()` 与 Editor Primitive 的 VAO 提交统一经过缓存。
- 不再在每次 Mesh Draw 后绑定 VAO 0。

优化后透明 Pass 保持 3 个 Draw，但实际只执行 1 次 VAO 绑定。Shader 请求基本都是一次请求对应一次变化；纹理仅有少量重复，当前场景收益不足以证明引入更广泛状态缓存的复杂度，因此暂不缓存 Program 和 Texture。

## 性能与限制

Recorder 只在 CPU 侧统计 Renderer 已接入的提交入口，不等同于 GPU 时间。它用于定位冗余 API 调用和验证排序效果；Pass GPU 时长、驱动内部开销和带宽瓶颈仍应以 RenderDoc Event Browser、Pipeline State 和 GPU Counter 为准。

当前日志在首帧或统计变化时输出，不逐帧刷屏。未来若引入 ImGui 性能面板，可以直接消费同一份 `PassSubmissionStats`，但需要先将只读快照接口从 Recorder 中公开。

各 Pass 的异步 GPU Timer Query、EMA 和只读快照已在后续阶段实现，生命周期与限制见 `docs/gpu-pass-profiling.md`。

## 多实例基线（2026-08-14）

新增默认关闭的 `--instance-grid N` 场景后，已完成 `1x1`、`8x8`、`16x16`
三档独立运行诊断。所有不透明实例共享同一 Mesh/Material；网格扩大到 256 个实例时，
Renderer 资源数仍保持 2 Mesh / 4 Material，主视图可见 259 个 Primitive 且无误裁剪。

16x16 场景的 RenderDoc 捕获为
`captures/maix_instances_16_frame146.rdc`，大小约 95.9 MiB。CLI 缩略图回放成功，
确认主视图网格和反射结果有效。Forward 数据为 258 Draw、Material `256/1`、
Mesh `258/3`、Texture `1030/9`，因此下一步优先实现共享资源不透明批次的
Instancing；Texture cache 在 Instancing 后重新评估。完整数据与 CPU/GPU 数据流见
`docs/instance-benchmark.md`。

## 不透明 Instancing 结果（2026-08-14）

主视图、反射视图和阴影视图现在消费按 Shader/Material/Mesh 稳定 ID 构建的不透明
批次。最终实现使用 Pass 自有的六区域流式 UBO，每实例仅上传 64 字节 model-view
矩阵；Forward GPU 由每视图 uniform 将观察空间位置继续变换到裁剪空间和灯光空间。
单个 16 KiB std140 块可移植地容纳 256 个实例。

优化后的 16x16 捕获为 `captures/maix_instanced_16_frame2.rdc`，大小约 95.8 MiB。
`renderdoccmd thumb` 回放成功，缩略图确认网格、反射和统计面板均有效。捕获帧中
Shadow 为 `1 Draw / 1 instanced`，Reflection 为 `2 / 1`，Forward 为 `3 / 1`；
对应优化前分别为 256、65、258 Draw。Forward Material 请求由 256 降为 1，
Texture 请求由 1030 降为 10。

GPU Timer Query 在本机存在 Reflection 周期性峰值，因此独立运行的最终 EMA 只作为
方向性快照。同构建 A/B 的稳定帧样本约为 Instancing `1.47-1.49 ms`、逐项 Forward/
Reflection 提交 `1.50-1.52 ms`。结论限定为：CPU/API 提交冗余已显著消除，最终紧凑
布局未显示明确 GPU 回归；不能据此宣称具有统计意义的 GPU 加速。详细数据和被淘汰
方案见 `docs/instance-benchmark.md`。
