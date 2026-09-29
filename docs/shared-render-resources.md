# 共享 Mesh 与 Material 资源

## 所有权与接口

Renderer 是场景 Mesh 与 Material 的唯一所有者。Application 通过以下接口创建资源和实例：

```text
CreateMesh(vertices) -> MeshHandle
CreateMaterial(material) -> MaterialHandle
AddPrimitive(meshHandle, materialHandle, transform, ...)
```

`MeshHandle` 与 `MaterialHandle` 是不同的强类型，避免调用侧混用资源类型。`AddPrimitive` 会在 CPU 侧验证句柄，再从 Renderer 资源表解析出指针并写入 `PrimitiveSceneProxy`。Scene Proxy 和后续 `RenderItem` 只保存非 owning 指针及稳定 ID，不参与 GPU 资源释放。

原有 `AddPrimitive(vertices, material, ...)` 作为便利接口保留，内部创建独立 Mesh/Material 后转调句柄接口。它适合一次性资源；需要实例化时应显式创建并复用 Handle。

`RemovePrimitive` 移除场景引用后，调用方可用 `DestroyMesh` / `DestroyMaterial` 释放不再使用的资源。只要任一 Primitive（包括不可见 Primitive）仍引用资源，销毁请求就返回 `false` 并输出诊断，资源和 GPU 对象保持有效。便利接口在后续创建步骤失败时回收已创建的资源；异常会在清理后继续向上抛出。

## GPU 数据流

Mesh 首次创建时上传一份 VAO/VBO。多个 Primitive 共享 Mesh 时，不透明标准三角形可按 Shader/Material/Mesh 批次进行 Instancing；透明与曲面细分路径仍逐项提交。Material Handle 可以独立或共享，纹理资源仍由 Material 内部的 `shared_ptr<Texture2D>` 管理。

透明验收场景中的三张平面现在共享一份 Mesh，并分别使用三份 Material：

```text
4 Primitive
├─ 茶壶：1 Mesh + 1 Material
└─ 三张透明平面：1 shared Mesh + 3 Material

总计：2 Mesh + 4 Material
```

## 当前限制

资源表的槽位只增不复用；销毁会清空槽位，因此旧 Handle 会被现有有效性检查拒绝，不会命中新资源。当前没有引用计数、空槽复用或 generation 校验。场景代理和 `RenderView` 中的资源指针仍是非 owning 指针；`RenderView` 仅供当帧同步执行，不能在移除 Primitive 并销毁资源后继续使用旧视图。

共享资源减少了重复 GPU Buffer 和 CPU 资源对象，但当前各 Resource 类仍自行绑定/解绑 OpenGL 状态。是否加入 Shader、VAO 和纹理状态缓存，应以 RenderDoc 捕获的实际状态切换为依据。
