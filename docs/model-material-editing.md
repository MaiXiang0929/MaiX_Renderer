# 模型与材质编辑联动

## 使用方式

在 Scene 或视口中选中模型，Inspector 的 Materials 区域会列出该模型实际使用的材质，并在各材质下显示使用它的分段名称。共享同一材质的多个分段在列表中合并为一项。

点击材质条目会打开并激活 Material Editor 标签，与 Inspector 共用最右侧停靠区域；点击 Inspector 标签可返回模型或灯光属性。`View > Material Editor` 也可以关闭或打开材质面板，打开时激活标签。默认范围为当前模型，面板顶部显示模型名称；Material 下拉框可以切换该模型的其他材质。切换模型会选择新模型的第一个有效材质，但保持当前标签，不抢占视口输入。选择灯光或取消模型选择会清空模型范围的材质选择。

材质字段采用上下排列并使用可用宽度；六个纹理槽位各自纵向显示预览、来源和 Select/Clear 操作。来源名称换行，悬停显示完整路径，内容通过面板滚动查看。

`All materials (debug)` 是显式的全场景调试入口，列出全部有效材质，并显示所属模型。无编辑模型关联的测试材质使用 `Scene` 前缀。此模式保持到取消勾选，或点击 Inspector 中的模型材质为止。

删除当前材质所属的模型后，不会自动选择剩余模型的材质。同一编辑范围内的选中资源失效时，也会清空选择，允许用户从下拉框显式选择其他有效材质。

## 数据流与所有权

- `EditableModelSection` 保存实际使用的 MaterialHandle 和分段名称；这份关联用于编辑器查询，不拥有 GPU 对象，也不承担资源清理。
- `EditableModel::materials` 继续记录本模型创建的资源句柄，用于模型删除和导入失败回滚；GPU Mesh/Material/Texture 仍由 Renderer 管理。
- Application 持有 `EditorMaterialSelection`。该状态保存 ModelId、MaterialHandle 和编辑范围，不保存数组下标或资源指针。Application 在 Scene 选择/删除后同步身份，再绘制 Inspector 与 Material Editor；材质面板关闭时同样执行失效清理。
- CPU 在模型范围内筛选实际使用且仍有效的材质。点击下拉框后立即重新获取当前句柄的快照；参数和纹理操作继续通过 Renderer 更新接口提交。
- GPU 继续在现有 Forward、Reflection、Outline 等 Pass 中读取材质参数和纹理。未新增 Shader 或 GPU 资源类型，材质修改会在后续渲染帧生效。
- 同一 MaterialHandle 的编辑影响所有引用它的分段；重复导入的两个模型当前使用各自独立的材质资源，因此可以分别编辑。

新增的中文注释集中说明选择规则、非 owning 关联、每帧同步时机，以及导入过程中名称分配与 Primitive 回滚的关系。

## 验收步骤

启动 `out/build/codex-multimodel/MaiX_Renderer.exe`，导入同一个 FBX 两次，将第二个模型移开后检查：

1. 选中第一份模型。Inspector 应显示材质名称和使用分段；点击其中一项应在 Inspector 原区域激活 Material Editor 标签，顶部模型名称与 Scene 一致，其他面板位置不变。
2. 修改该材质的 Base Color、Roughness 或 PBR/Toon 参数。应只改变第一份模型中使用该材质的分段，第二份模型保持原参数。
3. 切换到第二份模型。Material Editor 应立即显示第二份模型名称及其材质；手动切换 Material 下拉框后，参数应对应新材质。
4. 删除当前正在编辑的模型。材质面板应清空选择，不自动编辑另一份模型；选中剩余模型后仍可编辑。再次导入后，新模型也应能独立编辑。
5. 勾选 `All materials (debug)`，确认全场景材质可访问且包含所属模型名称。点击 Inspector 的材质，确认回到模型范围。
6. 使用 `--material-lab` 启动，选中 Material Lab，确认 Inspector 可分别选择 Copper、Plastic、Ceramic、RoughMetal；使用 `--translucency-test` 时，可通过全部材质入口继续编辑透明测试材质。
7. 来回切换 Inspector/Material Editor 标签，确认 Transform 与材质编辑均可使用；切换模型保持当前标签。关闭再从 View 菜单打开材质面板，应激活材质标签。缩窄右栏后参数和纹理操作仍可用；正常退出重启或 Reset Layout 后，两个面板仍共用同一区域。

## 已完成验证与限制

构建和 14 项现有测试通过。EditorInteractionTests 增加了共享分段去重、模型切换、删除前置列表元素后保持句柄、资源失效清空、模型删除、全局调试范围失效处理及选择灯光后的清理检查。默认、Material Lab、2×2 Instancing 和透明测试场景各启动运行 3 秒，错误日志为空。

材质面板停靠调整后再次构建，14 项测试通过；默认与 Material Lab 场景各运行 3 秒，未发现初始化或 Shader 错误。此轮没有自动操作标签，停靠、焦点与窄栏显示仍待用户验收。

仓库不包含实际双导入验收的外部 FBX；以上界面、画面及材质编辑效果**待用户验收**。本轮不包含材质重新分配、材质实例、复制、资产缓存或场景保存。
