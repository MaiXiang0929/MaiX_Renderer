# 显式 Pass 资源契约

## 目标与范围

本轮服务当前实时图形与渲染引擎的多 Pass 数据流、资源生命周期与诊断。固定执行顺序保持为 Shadow、Reflection、Forward、Outline、Translucency、SSAO、Bloom、PostProcess、EditorPrimitive、Present。契约已经接入实际资源访问，不只是描述表。

`PassResourceId` 是管线内的逻辑资源与内容阶段，区别于 Mesh/Material 使用的运行时 `RenderResourceId`。本轮没有完整 Render Graph、自动排序、Pass 裁剪、资源池、通用版本或跨 API 抽象。

## 源码参考与取舍

阅读的是此前浅克隆的固定提交，借鉴设计思路，按 MaiX 现有接口实现：

- Filament `cdb1f212ab1c944b9c171e594bcbab1e761087db`：`filament/src/fg/FrameGraphResources.cpp` 在取得资源时验证当前 Pass 已声明访问；`FrameGraph.cpp` 的写入逻辑产生内容版本。MaiX 采用受限访问入口与固定内容阶段，不复制完整 FrameGraph。缺少本帧生产者时拒绝消费，是本轮补充的规则，不能宣称该参考实现所有情况都已经检查。
- OGRE-Next `475f783d76f21b530e718aab4a548cdd9a222e50`：`OgreCompositorNode.cpp` 的输入/输出通道与重建通知，以及 `OgreCompositorPassDef.h` 的 Load/Store 描述。MaiX 采用清空/保留语义与每帧重新绑定，不加入完整 Compositor 和资源状态屏障系统。
- Canavar `230b44d4feb3592406f78f6aed47ea54cbdb93bd`：`PostProcessEffect::ApplyEffect(input, output)` 与 Renderer 的顺序后处理。MaiX 保留固定顺序，输入显式选择，不引入逐效果 Ping/Pong 拷贝。
- NPR-Studio `092bb7a3a3a62ab8eae2b3c51907b144fa27f794`：`gloo/Renderer.cpp` 与 `OutlineNode.cpp` 主要为顺序绘制及局部资源管理；未从这些模块移植通用契约或描边算法。

## CPU/GPU 数据流与所有权

1. Renderer 注入视图、绘制资源、环境纹理与设置。环境纹理属于外部输入；缺少环境纹理时保留现有跳过天空盒的行为。
2. Pipeline 先完成全部目标的尺寸检查/重建，再调用 `FrameResources::BeginFrame` 清空绑定与已生成状态，根据各资源所有者建立本帧快照。
3. `BuildPassResourceContract` 集中声明当前配置的输入、输出、访问方式以及 Clear/Preserve。初始化时检查阴影/SSAO/Bloom 的全部八种配置，仍采用现有固定顺序。
4. 每个 Pass 执行前，`PassResources::Validate` 检查必需内容、格式、尺寸、深度附件、保留源与目标关系，以及声明输入和输出附件的采样反馈。
5. Pass 通过 `Texture/Read/Output/BeginTarget/EndTarget` 访问资源。未声明的访问会抛出含 Pass 和资源名的 `ResourceContractError`。
6. Pass 提交绘制或明确保留原内容后调用 `Publish`；Pipeline 调用 `Complete` 检查所有声明输出已发布。
7. 只有 Present 发布 FinalColor 后，Pipeline 才向编辑器提供最终纹理；失败、零尺寸或 Resize 中断不会暴露该帧的旧输出。

各 Pass 仍拥有自身 Framebuffer/ShadowMap/Shader。绑定表仅保存整数快照，不复制、分配或释放 GPU 对象，不跨帧使用。Reflection 和 Translucency 对 Forward 的引用只用于复用材质/天空盒绘制，已不通过它取得主视图目标。SSAO 的合成尺寸由 Pipeline 显式传入。

CPU 只检查绑定与命令提交顺序；“本帧已生成”表示生产命令已经提交，并不表示 GPU 执行完成。GPU 继续使用原 Shader 的矩阵、观察空间深度重建、光照和后处理算法。不新增 glFinish、GPU 读回或强制同步。

## 资源流向

| Pass | 输入 | 输出及附件语义 |
|---|---|---|
| Shadow | 场景绘制数据 | ShadowDepth，深度 Clear |
| Reflection | Environment、条件 ShadowDepth | ReflectionColor，颜色/私有深度 Clear |
| Forward | Environment、ReflectionColor、条件 ShadowDepth | ForwardColor 与 ForwardDepth，颜色/深度 Clear |
| Outline | ForwardColor 保留、ForwardDepth 深度测试 | OutlinedColor，颜色/深度 Preserve |
| Translucency | OutlinedColor 保留、ForwardDepth 深度测试、Environment、条件 ShadowDepth | SceneHdrColor，颜色/深度 Preserve，透明绘制深度只读 |
| SSAO | SceneHdrColor、ForwardDepth 采样 | SsaoAO 与 SsaoColor，各自颜色 Clear |
| Bloom | 显式选定 HDR 输入 | Bloom；最后一次内部模糊覆盖输出，不清空 |
| PostProcess | 显式选定 HDR 输入、条件 Bloom | PostColor，颜色 Clear |
| EditorPrimitive | 灯光与视图数据 | Overlay，颜色 Clear 为透明，再绘制 |
| Present | PostColor、Overlay | FinalColor，颜色 Clear |

ForwardColor → OutlinedColor → SceneHdrColor 共享一套 RGBA16F 颜色附件和主深度。新阶段发布后旧颜色身份失效，避免读取与名称不符的已修改内容。没有描边对象、Tessellation 跳过描边或透明队列为空，仍传递阶段结果，不分配额外主颜色纹理。

SSAO 开启时，Bloom/PostProcess 的输入声明指向独立 SsaoColor；关闭时指向 SceneHdrColor。Bloom/PostProcess 原有的 Forward 隐式回退已移除。

## 关闭、重建与失败

- 阴影关闭：Shadow 不发布内容，消费者不声明阴影输入，Shader 接收禁用标记。
- SSAO 关闭：不发布 SsaoAO/SsaoColor；HDR 输入在契约构建时选择原颜色。
- Bloom 关闭：不发布 Bloom；PostProcess 不声明该输入并禁用合成。
- Overlay 关闭或无灯光：仍清空并发布透明 Overlay，避免残留旧 Gizmo。
- Resize：沿用事务式 Framebuffer 重建与失败后重试；成功后获取最新 ID/格式/尺寸。没有整条管线目标的原子交换。
- 契约失败：执行前缺少必需资源不提交该 Pass；异常后停止本帧后续 Pass，结束计时/统计并清空当前访问器。FinalColor 返回 0，编辑器显示视口未就绪。相同连续错误只打印一次，成功帧后解除去重。

Clear 命令暂时解除颜色/深度写掩码及 Scissor 对清空的限制，随后恢复写掩码和 Scissor 开关。Preserve 不发出清空命令。Mipmaps 延续各资源既有行为，主颜色在透明阶段结束后生成，Outline 不生成。

## 验证结果

2026-10-01：构建与 14 项现有测试通过。

- `RenderPassContractTests` 覆盖八种配置、显式 HDR 来源、跨帧清空、缺少生产者、Bloom 提前、未声明输入/输出、未发布结果、旧颜色身份失效、空绘制阶段传递、格式/尺寸/零纹理、保留源不匹配、采样反馈及外部资源冒充。
- `FramebufferTests` 使用实际隐藏 OpenGL 4.0 上下文，验证 Clear 在已有写掩码和 Scissor 下完整清空附件，恢复掩码/Scissor，并验证 Preserve 保留已有 HDR 颜色与深度；本次上下文可用，测试没有跳过。
- 默认、Material Lab、透明测试与 2×2 Instancing 各启动运行 3 秒，未发现初始化、Shader 或资源契约错误。

自动验证范围限于上述构建、测试与启动检查。2026-10-01，用户确认本轮视觉效果验收通过；该结论来自用户验收，不是助手自动观察画面的结果。

## 用户验收

启动 `out/build/codex-multimodel/MaiX_Renderer.exe`。可分别使用 `--material-lab`、`--translucency-test`、`--instance-grid 2`。

1. 默认场景检查物体、地面反射和阴影；View 打开 Renderer Statistics。
2. 往返切换 SSAO 与 Bloom 的四种组合；检查无黑屏、旧效果残留及 `[PassResources]` 错误。
3. 切换阴影和灯光 Gizmo，检查关闭后不残留，开启后可以恢复。缺少输出时查看终端诊断；完整诊断暂不转入编辑器 Console。
4. Material Lab 选择材质并切换 Toon/Outline，确认描边与颜色存在；按 `T` 切换 Tessellation，检查既有描边跳过规则，恢复后仍正常。
5. 透明场景检查主视图与反射中的混合；多模型导入、移除和材质编辑仍可用。
6. 调整 Viewport 停靠尺寸、最小化后恢复，检查图像、拾取及 Gizmo；正常路径不应出现资源尺寸错误。
7. 可使用 RenderDoc 核对各 Pass 的颜色/深度附件与输入纹理；无需依赖新界面或图形化依赖查看器。

**本轮视觉效果已由用户于 2026-10-01 确认验收通过。** 上述步骤保留为后续回归检查清单。

## 当前限制

- 验证覆盖经契约入口传递的跨 Pass 资源，不拦截任意原始 OpenGL 调用，也不检测 GL 对象被外部删除。
- Bloom 内部 Ping/Pong 与 SSAO 原始目标、内部采样仍由各 Pass 管理，没有拆成图节点。跨边界反馈检查按声明输入与导出目标进行，不声称覆盖每一次内部绘制。
- Shader 编译/链接诊断仍属于现有初始化与重载流程；Publish 不尝试检查 GPU 是否成功写入每个像素。
- 阴影仍是固定 2048² 单一深度贴图，资源 ID 与尺寸规则仅覆盖当前管线；未来新增目标需补充契约与绑定。
