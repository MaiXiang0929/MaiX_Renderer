# 文档重整范围与验证记录

重整日期：2026-10-01。原 `docs/` 文档按开发轮次组织，计划、实现与验收记录混合。本轮改为按当前子系统讲解实现，删除原文件，不保留历史计划副本；保留有价值的原性能测量到独立基准页。

## 原文件与新说明对应关系

| 原文件 | 新说明 |
| --- | --- |
| `frustum-culling.md`、`opaque-render-sorting.md` | [场景与视图](rendering/scene-and-views.md) |
| `pass-resource-contracts.md`、`render-target-resize.md` | [管线与资源](rendering/pipeline-and-resources.md) |
| `pbr-material-workflow.md` | [材质与着色](rendering/materials-and-pbr.md) |
| `material-instances.md`、`shared-render-resources.md` | [材质实例](rendering/material-instances.md)，所有权同时见[架构](architecture.md) |
| `translucency-pass.md`、`translucent-materials.md` | [反射与透明](rendering/reflection-and-translucency.md) |
| `npr-toon.md`、`npr-outline.md`、`npr-face-shadow.md` | [卡通与面部阴影](rendering/npr.md) |
| `ssao.md`、`bloom-postprocess.md`、`hdr-tone-mapping.md` | [后处理](rendering/postprocessing.md) |
| `model-texture-import-plan.md` | [实际导入实现](assets/model-import.md)，原替换模型计划改为当前追加模型行为 |
| `multi-model-scene.md`、`model-material-editing.md`、`material-editor.md` | [编辑器工作流](editor/workflows.md)与[材质实例](rendering/material-instances.md) |
| `viewport-transform-controls.md`、`editor-scene-inspector.md`、`editor-docking-layout.md` | [编辑器工作流](editor/workflows.md) |
| `editor-light-visualization.md` | [后处理叠加](rendering/postprocessing.md)与[编辑器拾取](editor/workflows.md) |
| `renderer-statistics-panel.md`、`gpu-pass-profiling.md` | [诊断](diagnostics/profiling-and-instancing.md) |
| `instance-benchmark.md`、`renderdoc-baseline.md` | [实例实现](diagnostics/profiling-and-instancing.md)与[原基准](diagnostics/benchmarks.md) |

新增多灯光与阴影、总架构和统一导航，补齐原文档中分散于其他主题的实现说明。根 README 与 AGENTS.md 中旧文档路径同步迁移。

## 本轮代码核实的关键边界

- 主颜色使用浮点高动态范围附件，反射仍为 8 位颜色附件。
- 环境反射是普通立方体纹理近似，不是完整预滤波环境照明。
- 主灯阴影矩阵由应用拟合透视投影，不能泛化为完整方向光阴影工具。
- 透明物体不写深度，也不进入当前阴影绘制。
- 遮蔽核固定在屏幕空间，半径参数筛选观察空间距离，合成作用于整场景颜色。
- 实例缓冲区域轮转不等于显式同步保证，提交统计没有独立 CPU 毫秒计时器。
- 包围球最大轴算法的保守性依赖当前变换约定，不覆盖任意剪切。
- 模型导入烘焙静态姿态，场景为会话内模型列表，无运行时骨骼和保存加载。

以上仅完善说明，本轮没有修改运行代码或修复算法。

## 本轮验证

| 检查 | 2026-10-01 结果 |
| --- | --- |
| 文档替换 | 原 27 份文件删除，新建 15 份中文文档 |
| 本地文档与源码链接 | 根 README、AGENTS.md 与新文档合计检查 129 个本地链接，无失效路径 |
| 构建 | `windows-ninja-debug` 构建成功，构建系统确认无待编译项；本轮未修改源码 |
| 自动测试 | 15/15 通过，包含真实 OpenGL 帧缓冲与材质实例检查 |
| 启动 | 默认、材质、材质实例、透明、16×16 实例网格均运行至少三秒并输出渲染统计 |
| 日志 | 五个场景标准错误输出为空，未发现明显初始化、着色器编译/链接或资源契约错误 |
| 变更格式 | 差异空白检查通过 |

测试工具最初无法写入已有日志目录，按项目范围权限运行后完成；旧文件部分删除也需要项目范围权限。无运行代码改动。启动检查属于短时稳定性检查，不验证长时间运行、所有开关组合、外部角色资产或实际交互。历史基准捕获没有重新回放。

## 用户画面与交互检查

视觉效果待用户验收。本轮文档不改变画面；以下入口用于对照实现说明：

```powershell
.\out\build\windows-ninja-debug\MaiX_Renderer.exe
.\out\build\windows-ninja-debug\MaiX_Renderer.exe --material-lab
.\out\build\windows-ninja-debug\MaiX_Renderer.exe --material-instance-lab
.\out\build\windows-ninja-debug\MaiX_Renderer.exe --translucency-test
.\out\build\windows-ninja-debug\MaiX_Renderer.exe --instance-grid 16
```

外部面部阴影场景需要模型、阈值贴图和精确材质名称，不能用占位路径完成有效启动。检查灯光从正面到侧面的面部边界、模型旋转后轴向、镜像与过渡；检查工作区标签、视口输入、双模型选择和材质同步。
