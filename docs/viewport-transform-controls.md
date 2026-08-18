# 编辑器视口与模型变换控制

状态：已实现，视觉与交互手感待用户验收

记录日期：2026-08-18

## 展示目标映射

本工作流对应最终展示的 `50-60s` 编辑器操作片段：分别选中模型或可见光源、修改对象变换，并让相同的世界空间状态立即进入 Forward、Reflection、Shadow、Translucency、视锥裁剪、Outline 和编辑器叠加层路径。

视口相机导航同时用于前面 PBR、NPR 和后处理展示片段的镜头构图。

## 操作方式

- 单击鼠标左键：点击茶壶选中模型，点击灯光图标选中对应光源；单击空白区域取消选择。
- `Alt` + 鼠标左键拖动：围绕相机观察目标旋转。
- `Alt` + 鼠标中键拖动：在观察平面内平移相机目标。
- `Alt` + 鼠标右键拖动或滚动滚轮：靠近或远离相机目标。
- `W`、`E`、`R`：切换移动、旋转和缩放操作器。
- `Q`：切换世界空间与局部空间操作模式。
- `F`：聚焦当前选中的模型或光源。
- `Ctrl` + 鼠标左键拖动：保留原有的主光源旋转操作。

`Transform` 面板与视口操作器共享当前选择状态。模型支持位置、旋转和缩放；光源当前只暴露有明确场景语义的位置移动，避免把缩放误映射到光照范围或强度。

## 所有权与数据流

`EditableModel` 与 `EditableLight` 是 CPU 侧的编辑器状态。统一选择类型为 `None / Model / Light`，因此模型和光源不会同时处于选中状态。`EditableModel` 持有面向美术操作的根 `Transform`，以及一组不拥有渲染资源的模型分段绑定。每个分段记录：

- `PrimitiveId`
- 分段局部矩阵 `sectionLocal`
- 局部空间包围体

Renderer 继续拥有 Mesh、Material 和对应 GPU 资源，编辑器不接管这些资源的生命周期。

`EditableLight` 保存稳定 `LightId`、名称、编辑器 Transform 与 `LightSceneProxy`。移动灯光后，CPU 调用 `Renderer::UpdateLight()` 更新渲染侧代理；之后灯光 UBO、阴影视图和光源调试图标在后续帧消费同一份位置。主聚光灯仍保持朝向模型中心，`Ctrl + 鼠标左键` 与 Gizmo 修改的是同一份 Transform，不会被每帧旧状态覆盖。

```text
鼠标 / Inspector / Gizmo
    -> 选择类型：Model
       -> EditableModel 根 Transform
       -> root * sectionLocal
       -> Renderer::UpdatePrimitiveTransform(PrimitiveId)
       -> PrimitiveSceneProxy::localToWorld
    -> 选择类型：Light
       -> EditableLight Transform
       -> Renderer::UpdateLight(LightId)
       -> LightSceneProxy
    -> BuildRenderView
    -> Shadow / Reflection / Forward / Outline / Translucency / EditorPrimitive Pass
```

视口导航只修改 CPU 侧的 View Matrix，不通过移动整个世界来模拟画面移动。`PresentPass` 直接在裁剪空间绘制全屏 Quad，因此屏幕空间拾取与最终显示共用同一套视口变换。

## 拾取流程

鼠标位置首先从 GLFW 窗口坐标换算为 framebuffer 像素坐标，然后通过 View-Projection Matrix 的逆矩阵反投影，得到世界空间射线。

CPU 使用该射线依次检测模型各分段经过变换后的保守包围球。Point 与 Spot 灯光的 Billboard 固定为 28 像素，因此灯光拾取采用“世界位置投影到屏幕 + 固定像素半径”的检测方式，命中范围不会随相机距离变化。灯光图标与模型重叠时，灯光图标优先；多个图标重叠时选择深度更靠前的光源。单击空白区域时清除当前选择。

Directional Light 没有世界空间位置，当前也没有可点击 Billboard，因此本轮不支持直接点击选择方向光；后续应在引入方向光专用图标和旋转编辑语义后再接入。

本轮只实现形成可展示结果所需的 CPU 包围球拾取。以下能力明确留到后续：

- GPU Object ID Picking
- 三角形级精确拾取
- 多选
- 场景层级编辑
- 网格或角度吸附
- Undo / Redo
- 自由飞行相机

## CPU 与 GPU 职责

CPU 负责：

- 处理鼠标和键盘输入。
- 维护相机状态、模型根 Transform、灯光 Transform 与互斥选择状态。
- 构造世界空间拾取射线。
- 计算 `root * sectionLocal`。
- 更新 `PrimitiveSceneProxy::localToWorld`。
- 更新 `LightSceneProxy` 的位置与主聚光灯方向。
- 重建各视图的可见项和排序结果。

GPU 负责：

- 使用更新后的 Model、View、Projection Matrix 执行顶点变换。
- 执行光栅化、深度测试、材质着色和纹理采样。
- 在 Shadow、Reflection、Forward、Outline 和 Translucency Pass 中消费相同的模型世界矩阵。
- 在灯光 UBO、Shadow 和 EditorPrimitive Pass 中消费更新后的灯光代理。

模型移动或缩放不会重建 Mesh、Material、VAO 或纹理，只更新 CPU Transform 和后续 Draw 使用的矩阵数据。

## 验证方式

自动化测试覆盖：

- 相机 Target、世界位置与 View Matrix 的一致性。
- Pan、Dolly、Focus 和 Pitch Clamp。
- 根 Transform 的矩阵组合顺序。
- 屏幕中心世界射线构造。
- 模型分段包围球命中与未命中。
- 灯光固定像素图标命中，以及灯光与模型重叠时的选择优先级。
- 灯光、模型与空白区域三种互斥选择结果。
- 多分段世界空间包围体合并。

构建和启动验证负责确认程序能够运行，并检查明显的 OpenGL 初始化、Shader 编译或链接错误。

以下视觉结果由用户验收：

- 单击茶壶和两个可见光源能否分别、互斥地选中。
- 灯光图标覆盖茶壶轮廓时，能否优先选中灯光。
- 移动光源后，灯光图标、光照和主灯阴影是否跟随。
- Gizmo 拖动方向和操作手感是否符合预期。
- 移动、旋转和缩放是否作用于整个多分段模型。
- 阴影、反射和描边是否正确跟随模型。
- 不同资产尺度下的相机导航速度是否合适。
