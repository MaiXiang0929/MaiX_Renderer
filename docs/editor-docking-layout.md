# Editor Docking Layout

## 当前目标

服务当前实时图形与渲染引擎的编辑器工作流：让场景画面、Scene、Inspector、资产和 Console 同时可见，并在视口内调整模型或灯光。Material Editor 与 Inspector 共用最右侧区域，通过标签切换，避免浮动窗口遮挡场景操作。布局与画面效果待用户视觉验收。

## 布局与资源边界

- `EditorWorkspace` 创建主 DockSpace，布局版本更新后首次运行或选择 `View > Reset Layout` 时生成默认布局；此后保留用户调整。停靠状态由 ImGui 保存至 `%LOCALAPPDATA%/MaiX_Renderer/imgui.ini`；无法创建该目录时使用当前工作目录的 `imgui.ini`。
- 默认布局将 Material Editor 停靠到 Inspector 的同一节点；Viewport、Scene、Console、Content Browser 的节点及分区比例沿用已有布局。此次保留 `MaiXEditorDockspaceV2`，不删除整个停靠配置。每次运行首次打开 Material Editor 时，仅将它对齐到现有 Inspector 节点；此后本次运行中的手动拖动不被每帧覆盖。
- 点击 Inspector 材质条目或从 `View` 打开 Material Editor 时，只请求一次标签激活。切换场景模型只同步材质选择，不主动切换标签或抢占视口焦点。材质参数采用标签在上、控件在下的排列，纹理槽位采用纵向条目，来源文字自动换行，完整路径可悬停查看。
- `PresentPass` 拥有最终 RGBA8 颜色纹理，合成后处理画面和编辑器 Overlay；`Renderer` 只向编辑器暴露当前纹理 ID。视口仅在这一帧采样它，不能跨 Resize 缓存 ID。
- `Application` 先创建 ImGui 帧并取得 Viewport 内容矩形，再据此设置相机宽高比和 `RenderFrameData.viewportWidth/Height`。`RenderPipeline` 依照这个像素尺寸调整 GPU 目标。
- `EditorViewportRegion` 保存屏幕坐标及像素尺寸；GLFW 输入先相对图像左上角换算，再交给拾取。ImGuizmo 使用同一矩形。
- Content Browser 只浏览现有 `assets` 文件夹，PNG 预览下采样到 64×64，最多缓存 24 张，并在切换目录或退出时释放；Import FBX 仍调用原有异步导入流程。
- Console 目前只显示有限数量的启动、导入和 Shader 重载事件；Shader 编译器的完整诊断仍在终端输出。

## 操作与验收

1. 启动 `out/build/windows-ninja-debug/MaiX_Renderer.exe`，确认主窗口标题为 `MaiX Engine`。默认应出现左侧上方 Viewport、左下方共用标签区的 Console 和 Content Browser，以及右侧并排且占满高度的 Scene 和 Inspector。
2. 选中模型，点击 Inspector 的 Materials 条目：Material Editor 应在 Inspector 原区域作为标签激活，不覆盖 Scene 或视口。点击 Inspector 标签应能返回 Transform 和 Materials；再次点击材质应切回材质标签。Renderer Statistics 仍由 `View` 菜单打开。
3. 在视口内按 `Alt + 左键/中键/右键` 导航，点击模型或灯光并用 `W/E/R` 与 Gizmo 调整；在其他面板拖动时，场景相机不应移动。
4. 改变窗口和停靠面板尺寸，检查图像比例、拾取位置与 Gizmo 重合；检查 Bloom、Tone Mapping 和灯光 Gizmo 的显示。
5. 保持 Material Editor 标签激活，切换模型，检查编辑目标更新且当前标签不变。切回 Inspector 后切换模型，检查标签仍停留在 Inspector；通过 `View > Material Editor` 关闭再打开，检查材质标签重新激活。
6. 调窄右侧区域，滚动检查 PBR/Toon 参数及全部六个纹理槽位；来源名称应换行，Select/Clear 可点击。宽度不足时两按钮分行排列。
7. 正常关闭后重启，再打开 Material Editor：检查它仍与 Inspector 共用区域，其他面板布局保留。选择 `View > Reset Layout`，检查恢复原分区比例，两个面板仍共用标签区。

助手负责确认构建、启动、资源与 Shader 初始化没有明显错误。布局和画面视觉结果由用户验收。

2026-10-01：构建及 14 项现有测试通过；默认与 Material Lab 场景各启动运行 3 秒，未发现初始化、Shader 编译或链接错误。此验证未操作材质标签，标签激活、布局恢复、窄栏内容及画面结果仍待用户验收。
