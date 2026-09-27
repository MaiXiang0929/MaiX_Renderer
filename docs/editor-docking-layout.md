# Editor Docking Layout

## 展示目标

本轮服务 60 秒 Demo 的 50–60 秒编辑器段落：让场景画面、Scene、Inspector、资产和 Console 同时可见，并在视口内调整模型或灯光。布局与画面效果待用户视觉验收。

## 布局与资源边界

- `EditorWorkspace` 创建主 DockSpace，布局版本更新后首次运行或选择 `View > Reset Layout` 时生成默认布局；此后保留用户调整。停靠状态由 ImGui 保存至 `%LOCALAPPDATA%/MaiX_Renderer/imgui.ini`；无法创建该目录时使用当前工作目录的 `imgui.ini`。
- `PresentPass` 拥有最终 RGBA8 颜色纹理，合成后处理画面和编辑器 Overlay；`Renderer` 只向编辑器暴露当前纹理 ID。视口仅在这一帧采样它，不能跨 Resize 缓存 ID。
- `Application` 先创建 ImGui 帧并取得 Viewport 内容矩形，再据此设置相机宽高比和 `RenderFrameData.viewportWidth/Height`。`RenderPipeline` 依照这个像素尺寸调整 GPU 目标。
- `EditorViewportRegion` 保存屏幕坐标及像素尺寸；GLFW 输入先相对图像左上角换算，再交给拾取。ImGuizmo 使用同一矩形。
- Content Browser 只浏览现有 `assets` 文件夹，PNG 预览下采样到 64×64，最多缓存 24 张，并在切换目录或退出时释放；Import FBX 仍调用原有异步导入流程。
- Console 目前只显示有限数量的启动、导入和 Shader 重载事件；Shader 编译器的完整诊断仍在终端输出。

## 操作与验收

1. 启动 `out/build/windows-ninja-debug/OpenGL_Project.exe`，确认主窗口标题为 `MaiX Engine`。默认应出现左侧上方 Viewport、左下方共用标签区的 Console 和 Content Browser，以及右侧并排且占满高度的 Scene 和 Inspector。
2. 在 `View` 菜单中打开 Material Editor 或 Renderer Statistics；拖动面板后重启，检查布局恢复。选择 `Reset Layout` 检查默认布局恢复。
3. 在视口内按 `Alt + 左键/中键/右键` 导航，点击模型或灯光并用 `W/E/R` 与 Gizmo 调整；在其他面板拖动时，场景相机不应移动。
4. 改变窗口和停靠面板尺寸，检查图像比例、拾取位置与 Gizmo 重合；检查 Bloom、Tone Mapping 和灯光 Gizmo 的显示。

助手负责确认构建、启动、资源与 Shader 初始化没有明显错误。布局和画面视觉结果由用户验收。

本轮重新构建与启动通过；保存的停靠配置显示 Console 和 Content Browser 共用同一节点，Scene 与 Inspector 各自占满右侧高度。画面视觉结果仍待用户验收。
