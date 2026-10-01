# Material Instance：单层继承与覆盖

## 目标与参考

本轮对应当前实时渲染引擎的材质工作流、资源生命周期与多视图一致性。基础材质已有 PBR/Toon 参数和纹理绑定；实例在此基础上提供独立覆盖与恢复继承，不修改 Shader 算法。

阅读此前本地固定提交的源码，按 MaiX 现有边界实现：

- [Filament MaterialInstance](https://github.com/google/filament/blob/cdb1f212ab1c944b9c171e594bcbab1e761087db/filament/src/details/MaterialInstance.cpp)：实例引用父 Material，保存独立参数、纹理绑定和排序身份。默认实例复制不等于本轮动态逐项继承；动态继承是 MaiX 的编辑需求。
- [OGRE-Next HlmsDatablock](https://github.com/OGRECave/ogre-next/blob/475f783d76f21b530e718aab4a548cdd9a222e50/OgreMain/src/OgreHlmsDatablock.cpp)：clone 创建独立身份，共享部分带引用计数的状态并复制参数；借鉴共享与身份边界，不移植 HLMS。
- [Canavar TexturedModel](https://github.com/berkbavas/CanavarGraphicsEngine/blob/230b44d4feb3592406f78f6aed47ea54cbdb93bd/Canavar/Engine/Source/Canavar/Engine/Model/TexturedModel/TexturedModel.cpp)：对象显式提交自己的 PBR 参数；所读模块没有完整父材质继承系统。
- [NPR-Studio Material](https://github.com/Obi-Nnamdi/NPR-Studio/blob/092bb7a3a3a62ab8eae2b3c51907b144fa27f794/gloo/Material.hpp)：普通光照和 NPR 参数保存在材质中，纹理使用共享引用；借鉴同一有效材质服务各渲染路径。

## 数据、身份与所有权

Renderer 拥有 MaterialResourceStore。基础材质和实例统一使用现有 MaterialHandle，不复用槽位，不新增 GUID。

- MaterialResource 保存有效 Material 和可选 MaterialInstance。
- MaterialInstance 保存基础材质父句柄、21 组参数的显式覆盖标记/值，以及六个纹理槽的状态。
- 实例只能引用基础材质，不能形成嵌套、循环或更换父材质。
- MaterialResource 使用 unique_ptr；更新有效 Material 内容时不替换对象地址。PrimitiveSceneProxy/RenderItem 只持有 const Material 非 owning 指针。
- Store 检查实例对父材质的依赖；Renderer 的 DestroyMaterial 额外检查所有 Primitive 引用，包括不可见对象。
- Application 的模型清理先注销 Primitive，再释放实例和基础材质。Renderer 退出时也先释放实例。
- 纹理仍通过 shared_ptr<Texture2D> 共享。创建实例不复制 Mesh、Shader 或 GPU 纹理。

基础材质变更时，Store 先构建父子候选结果，所有可能抛异常的名称/纹理标签分配完成后再提交；有效材质和实例元数据的提交使用不抛异常的移动赋值。队列同步的跟踪列表同样在基础材质提交前分配。

## 参数与纹理语义

有效参数由基础材质和启用的覆盖项合并。父材质修改立即更新未覆盖项，已经覆盖的字段保持本地值；取消 Override 后恢复父材质当前值，而非创建时的旧副本。

覆盖包含 Base Color、Legacy Specular Color/Shininess、Environment Reflectivity、Metallic、Roughness、AO、Normal Scale、Opacity，以及 Toon、Rim、Face Shadow、Outline 参数。Face Forward/Right 为一组，合并后执行已有规范化，保持正交单位轴。创建、基础材质更新和实例解析使用同一参数规范化函数。

Shading Model 与 Blend Mode 只能从基础材质继承。基础材质 Blend Mode 修改时，Renderer 同步父材质及其实例对应的 Primitive，下一次主、反射与阴影视图构建使用新分类。实例 Opacity 可独立修改，但不会自动切换 Blend Mode。

| 纹理状态 | 行为 | 操作 |
|---|---|---|
| Inherit | 使用父材质当前纹理 | Reset to Parent |
| Replace | 使用实例自己的纹理引用 | Select |
| Disabled | 明确禁用该槽，即使父材质有纹理 | Clear |

所有六个槽位继续执行 Base Color=sRGB、其它槽位=Linear 校验。Reset All Overrides 同时重置参数和纹理状态。

## CPU 与 GPU 路径

编辑器操作 → Renderer/Store 校验与合并 → 稳定地址的有效 Material → Scene 视图分类与排序 → Pass 绑定 → GPU。

Forward、Reflection、Translucency、Outline 以及 Shadow 的位移路径读取同一有效 Material。现有 Uniform、纹理槽、局部 Face 轴和观察空间描边算法保持原路径；实例不改变坐标空间。实例不新增 GPU Buffer，也不引入 GPU 同步或读回。

每个实例拥有独立 MaterialHandle/排序 ID。只有相同 Shader、Mesh 和同一材质句柄的对象可以按现有规则 Instancing；不同实例即使参数相等也不合并。CPU 会在修改父材质时遍历资源并重算相关实例，因此本轮不承诺性能提升或 Draw Call 减少。

## 模型替换与编辑器

Material Editor 保留右侧标签布局：

1. 选择基础材质，Create Instance 创建实例，并替换当前模型中使用该材质的全部分段。
2. 实例显示类型、父名称、各字段 Override 与 Local/Inherited；未覆盖的控件只读，启用覆盖后才可修改。
3. Reset All Overrides 恢复全部继承；纹理分别提供 Reset to Parent。
4. Edit Parent 显式切换编辑目标，界面提示该操作会影响继承实例。实例选择不变，取消后返回实例编辑。
5. Use Parent Material 恢复当前模型相关分段，销毁已经没有 Primitive 引用的旧实例并从模型清理列表移除。
6. All materials (debug) 可编辑基础材质/实例；模型绑定操作仅在模型范围内可用。

ReplacePrimitiveMaterials 先验证全部 Primitive 存在且旧材质符合预期，验证完成后一次提交指针、材质 ID 和分类。模型创建前预留跟踪空间；创建/绑定失败保持旧分段并回收新实例。当前没有任意跨模型材质分配界面。

## 验收场景

启动：

```powershell
& 'D:/Code/Projects/MaiX_Renderer/out/build/codex-multimodel/MaiX_Renderer.exe' --material-instance-lab
```

场景只有一个可编辑模型 Material Instance Lab，包含三个共享 Mesh 的分段。Inspector/Material 下拉框分别选择 Parent Surface、Instance A、Instance B；A 初始覆盖红色 Base Color，B 全部继承。三个对象使用已有 2×2 布局的三个位置。全部资源由该模型跟踪，删除它会按依赖顺序清理。

用户检查：

1. 选择 Parent Surface，调整 Roughness；A/B 应随父材质变化。
2. 修改父 Base Color；A 保持本地颜色，B 继承父颜色。选择 A 取消 Base Color 的 Override，应立即恢复继承；再次启用后可独立修改。
3. 选择 B 覆盖 Roughness；父 Roughness 再变化时 B 保持自己的值。Reset All Overrides 后恢复父值。
4. 使用 Select/Clear/Reset to Parent 验证纹理三态；检查另一实例仍使用原有有效纹理。
5. 实例 Shading Model/Blend Mode 为只读；用 Edit Parent 修改它们，检查 PBR/Toon、Outline 和透明分类。透明与 Tessellation 描边仍遵循原有范围：仅主视图不透明 Toon 且非 Tessellation。
6. 观察主视图、反射以及 Normal/Displacement/Face Shadow 等已有路径。按 T 往返切换 Tessellation，确认恢复后仍能编辑。
7. 普通模式导入 FBX：选中材质并 Create Instance，编辑、重置、Use Parent Material，再删除模型。重复导入仍创建独立基础材质，不自动跨模型共享父材质。
8. 调整视口尺寸、最小化/恢复，检查无资源契约错误。

## 已完成验证

2026-10-01：构建与 15 项测试通过。MaterialInstanceTests 使用生产 Store、RenderScene、Renderer 和实际隐藏 OpenGL 上下文，CPU/GPU 检查均执行，没有跳过 GPU 部分。

覆盖：继承更新、单项写入隔离、参数规范化、Face 轴规范化、单项/全部重置、无效/嵌套父材质、稳定地址、父子销毁、原子替换失败、队列与批次身份、实际 Renderer 分类同步、共享 Primitive 最后一次释放、真实纹理 ID 共享、颜色空间拒绝、纹理三态、纹理回收和资源计数归零。

默认、Material Lab、透明场景、2×2 Instancing 和 Material Instance Lab 均启动运行 3 秒，未发现初始化、Shader 或 Pass 资源契约错误。此检查没有自动操作编辑器控件或检查画面。

**2026-10-01：用户确认第四轮 Material Instance 验收成功，视觉及交互效果验收通过。** 此结论来自用户验收；助手的验证范围仍为上述构建、测试与启动检查。

## 本轮范围

不含嵌套继承、重新指定父材质、实例独立管线模式、材质节点图、持久化/缓存、通用资产引用计数、Undo/Redo、材质 UBO、Bindless 或跨实例合批。不同模型的导入资源仍独立；Store 的线性父子更新适用于当前规模，未来优化需实际性能证据。
