# 固定渲染管线与资源生命周期

实现入口：[RenderPipeline](../../src/Renderer/Pipeline/RenderPipeline.cpp)、[资源契约](../../src/Renderer/Pipeline/PassResourceContract.cpp)、[目标操作](../../src/Renderer/Pipeline/PassResourcesGL.cpp)、[Framebuffer](../../src/Renderer/Resources/Framebuffer.cpp)。

## 阶段顺序与输入输出

Pass（渲染阶段）的顺序在管线构造函数中固定，功能开关改变资源契约和阶段内部行为，不进行拓扑排序。

| 顺序 | 阶段 | 主要输入 | 输出及处理 |
| --- | --- | --- | --- |
| 1 | `ShadowPass` | 阴影视图、位移数据 | 清空并写入 `ShadowDepth` |
| 2 | `ReflectionPass` | 反射视图、环境、可选阴影 | 清空并写入 `ReflectionColor` |
| 3 | `ForwardPass` | 主视图、环境、反射、可选阴影 | 清空并写入 `ForwardColor` / `ForwardDepth` |
| 4 | `OutlinePass` | 前向颜色保留、深度测试 | 原附件继续形成 `OutlinedColor` |
| 5 | `TranslucencyPass` | 轮廓颜色保留、深度、环境与阴影 | 原附件继续形成 `SceneHdrColor` |
| 6 | `SSAOPass` | 场景颜色、前向深度 | 启用时生成遮蔽与 `SsaoColor` |
| 7 | `BloomPass` | 按开关选择的场景颜色 | 启用时生成光晕纹理 |
| 8 | `PostProcessPass` | 场景颜色、可选光晕 | 曝光、色调映射、输出编码后的 `PostColor` |
| 9 | `EditorPrimitivePass` | 灯光等编辑数据 | 清空并生成独立 `Overlay` |
| 10 | `PresentPass` | 后处理颜色与叠加层 | 最终视口纹理 `FinalColor` |

`ForwardColor`、`OutlinedColor`、`SceneHdrColor` 是同一个物理颜色附件的三个内容阶段，绑定到同一个 FBO（帧缓冲对象）。换名称表示执行进度，不分配三个场景颜色纹理。

## 为什么需要资源契约

仅传递纹理编号不能说明该纹理本帧是否产生、是否可采样、尺寸是否匹配，以及当前阶段应清空还是保留原附件。

`PassResourceContract` 声明输入、输出、访问方式及 `AttachmentLoad`（附件载入方式）。采样、保留颜色、深度测试、颜色写入、深度写入分别描述用途。保留颜色不意味着将正在写入的颜色附件同时作为采样纹理。

`FrameResources` 是每帧 CPU（中央处理器）绑定快照，不拥有 GPU（图形处理器）对象。`PassResources` 限制当前阶段取得已声明的资源。执行流程为：

```text
检查全部目标尺寸
    → 建立本帧绑定表
    → Validate：资源存在、生产者、格式、尺寸、附件关系、采样反馈
    → BeginTarget：绑定目标并按声明清空/保留
    → 执行 OpenGL 绘制
    → EndTarget：结束目标，按需要生成纹理层级
    → Publish：宣布本帧内容有效
    → Complete：检查承诺的输出已发布
```

初始化验证阴影、屏幕空间遮蔽和光晕的八种开关组合。禁用功能不声明其输出，下游选择正确的替代输入。轮廓无绘制时仍发布颜色阶段，表示内容已经过该步骤。

契约错误会终止后续阶段、恢复默认目标并返回空最终纹理，避免展示上一帧的旧内容。它不实现完整 Render Graph（渲染图）、自动屏障、资源池或任意 OpenGL 调用拦截；阶段内部双缓冲仍由阶段管理。

## 目标规格与所有者

HDR（高动态范围）颜色允许保存大于 1 的线性光照值。

| 所有者 | 规格 | 分辨率 |
| --- | --- | --- |
| 阴影阶段 | 24 位深度纹理 | 固定 2048×2048 |
| 反射阶段 | RGBA8（四通道、每通道 8 位）及深度附件 | 视口宽高各约一半 |
| 前向阶段 | RGBA16F（四通道、每通道 16 位浮点）与可采样深度/模板 | 视口全分辨率 |
| 遮蔽阶段 | 两张 R8（单通道 8 位）与 RGBA16F 合成目标 | 遮蔽半分辨率，合成全分辨率 |
| 光晕阶段 | 高亮及两张 RGBA16F 模糊目标 | 半分辨率 |
| 后处理、编辑叠加、最终合成 | RGBA8 | 全分辨率 |

尺寸取中央视口像素尺寸，包含界面显示比例换算，不直接使用整个窗口大小。零尺寸跳过管线；半分辨率计算由 [RenderTargetSizing](../../src/Renderer/Pipeline/RenderTargetSizing.h) 统一处理。

## 尺寸变化与失败回退

`Framebuffer::Init()` 在规格一致时复用原对象。需要重建时先生成候选帧缓冲、颜色和深度附件，检查完整性，成功后交换到正式对象；失败时候选对象析构，旧目标保留。保存的绘制/读取帧缓冲、纹理与渲染缓冲绑定在退出时恢复，成功替换时将旧绑定映射到新对象。

这一事务只覆盖单个 Framebuffer（帧缓冲封装）。管线逐个更新目标，光晕和遮蔽内部也可能部分更新；任一失败使本帧停止渲染，下一帧根据全部内部目标的规格检查重试。它不是整条管线一次性原子替换。

连续拖动视口可能造成重复 GPU 分配；当前没有尺寸变化防抖或目标池。纹理层级生成也有成本，应根据实际捕获分析，不能仅从目标格式推断性能。

## 验证

[RenderPassContractTests](../../tests/Renderer/Pipeline/RenderPassContractTests.cpp) 检查纯逻辑资源契约；[FramebufferTests](../../tests/Renderer/Resources/FramebufferTests.cpp) 使用真实 OpenGL 验证创建、失败回退及清空/保留；[RenderTargetSizingTests](../../tests/Renderer/Pipeline/RenderTargetSizingTests.cpp) 检查尺寸策略。

运行时分别开关阴影、遮蔽和光晕，调整视口大小并最小化窗口，检查初始化、资源契约和帧缓冲错误日志。画面连续性与显示结果待用户验收。
