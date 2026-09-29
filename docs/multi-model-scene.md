# 多模型场景：本轮实现与验收

## 目标与边界

Scene 可以同时保存默认模型和多次导入的 FBX。每次导入得到会话内唯一的 `ModelId`，可独立选择、移动、聚焦和移除；Scene 只显示模型名称，不显示 ID 或列表序号。同名模型显示为 `名称`、`名称_001`、`名称_002`；删除后再次导入会使用当前可用的最小后缀。相同 FBX 导入两次会得到两个模型对象；初始 Transform 相同，因此它们起初可能完全重叠。

本轮不实现跨会话 GUID、场景文件保存/加载、模型层级、跨模型 GPU 资源去重或材质实例系统。

## 数据流与所有权

1. `AssetImportPanel` 在后台解析 FBX 的 CPU 数据，`Application::Update` 在渲染线程调用 `CommitImportedModel`。
2. `CommitImportedModel` 为本次导入建立 Material、Mesh 和 Primitive，全部成功后才向 `m_Models` 追加 `EditableModel`。失败时只回滚本次创建的资源，已有模型继续保留。
3. `EditableModel` 保存根 Transform、分段 Primitive ID，以及 Mesh/Material Handle。GPU 对象由 `Renderer` 持有；模型只记录清理所需的句柄。
4. Inspector 或 Gizmo 改动选中模型的 Transform 后，`ApplyEditableModelTransform` 将根矩阵与各分段局部矩阵组合，再更新对应的渲染侧 Primitive。GPU 在现有 Shadow、Reflection、Forward 等 Pass 中消费各 Primitive 的变换与材质。
5. 移除模型时先移除它的全部 Primitive，再销毁对应 Mesh/Material，最后从 `m_Models` 移除。其他模型的 ID、变换和资源不受影响。
6. 视口拾取遍历模型分段的世界空间包围球，选择沿射线最近的模型；灯光图标仍优先。场景总包围球用于主灯目标与阴影范围，地面高度根据各模型包围球保守估计。

`ModelId` 不使用指针或数组下标作为身份。`m_Models` 扩容或删除其他模型后，当前选择仍按 ID 查找。

## 用户验收

启动 `out/build/codex-multimodel/MaiX_Renderer.exe`，使用你自己的可导入 FBX：

1. 通过 `File > Import FBX...` 导入同一个 FBX 两次。Scene 应显示两条模型记录，默认模型仍在。第二次导入后，Inspector 应显示第二个模型。
2. 两次导入的模型起初可能重叠。将第二个模型的 `Position X` 改成足够大的值，或按 `W` 用 Gizmo 移动它。Viewport 应出现两个可区分的模型；第一个模型位置应保持不变。
3. 在 Scene 分别点选两条导入记录，改变各自的 Transform，并按 `F` 聚焦。Inspector、Gizmo 与视口选择应跟随当前模型。
4. 选中其中一个导入模型，点击 `Remove selected model`。对应模型应消失，另一模型仍能选择和编辑；Scene 不应出现 `#序号`。再导入同一个 FBX，应复用当前空出的名称后缀，内部 `ModelId` 仍是新的。
5. 检查 Console 与终端没有导入失败、Shader 编译/链接或 GPU 资源错误。

代码侧已完成构建、14 项测试和默认场景启动检查。以上画面与交互结果**待用户验收**。仓库不包含验收用 FBX，因此本轮没有自动执行实际 FBX 双导入。

## 当前限制

- 拾取采用分段包围球，重叠或细长网格可能出现粗选；灯光图标覆盖模型时优先选择灯光。
- 场景没有序列化，重启后 `ModelId` 会重新分配。
- 每次导入各自分配 GPU Mesh/Material，未复用相同资产。
- 视图、阴影和地面仍按全场景保守包围范围工作；极远距离分布的模型可能降低阴影精度。
