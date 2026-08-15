# Translucency Pass

## 管线位置

主视图透明物体按以下顺序进入管线：

```text
ForwardPass（不透明颜色与深度）
    -> TranslucencyPass（透明颜色混合，保留不透明深度）
    -> BloomPass
    -> PostProcessPass
    -> EditorPrimitivePass（独立 Overlay Buffer）
    -> PresentPass
```

首版只渲染主视图透明物体。反射视图仍只消费不透明列表，避免在透明路径稳定前扩大反射目标、排序和 mipmap 生命周期的耦合范围。

## CPU 数据流

`PrimitiveSceneProxy::translucent` 决定物体进入 `RenderView::translucentItems`。`BuildRenderView()` 使用变换后的世界空间包围球中心，并通过当前 `RenderView::view` 转换到观察空间。OpenGL 相机朝向观察空间 `-Z`，因此按 Z 值升序即可得到从后向前的提交顺序。

排序采用 `std::stable_sort`。当两个包围球中心深度相同时保留场景提交顺序，减少相机或浮点误差导致的顺序抖动。该方法仍是物体级近似，无法解决相交透明几何和单个网格内部三角形顺序问题。

## GPU 状态

`TranslucencyPass` 复用 `ForwardPass` 的 PBR Shader、灯光 UBO、阴影和材质绑定：

- 深度测试开启，使透明物体能被不透明几何遮挡。
- 深度写入关闭，避免先绘制的透明层遮挡后续透明层。
- 混合使用 `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA`。
- Pass 结束后恢复进入 Pass 前的深度与混合状态。

`MaterialProperties::opacity` 作为面向美术的标量上传给 PBR fragment shader，并被限制到 `[0, 1]`。物体是否进入透明路径仍由 Primitive 的 `translucent` 标志决定，避免仅修改透明度就隐式改变管线分类。

## 资源生命周期

透明 Pass 重新绑定 `ForwardPass` 拥有的颜色/深度 Framebuffer，不创建第二份目标。Forward Pass 绘制完不透明物体后只解绑；透明内容完成后才生成颜色纹理 mipmap，确保 Present 和后续消费者看到完整结果。

## 运行时验收场景

`Application::CreateTranslucencyTestScene()` 根据载入模型的直径动态创建三张竖直测试平面：

- 青色后层：`opacity = 0.28`
- 洋红中层：`opacity = 0.42`
- 黄色前层：`opacity = 0.58`

三层沿世界空间 Z 轴分布并略微横向错开。初始相机位于 `+Z`，因此黄色层最靠近相机；相机环绕后，排序应根据新的观察空间深度自动变化。平面不投射阴影，避免将透明混合验收与尚未实现的 Alpha 阴影混在一起。

三张平面复用同一个 `MeshHandle`，因此只创建一份 VAO/VBO；每层保留独立 `MaterialHandle`，用于设置颜色和透明度。

验收时观察重叠区域的颜色组合、主模型对透明层的遮挡，以及相机环绕过程中是否发生整层突兀消失。当前物体级排序不保证相交透明网格或单个网格内部三角形的正确顺序。
