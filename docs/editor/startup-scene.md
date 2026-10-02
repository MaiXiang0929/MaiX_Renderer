# 默认启动场景与独立相机

实现日期：2026-10-03。此步骤对应当前渲染引擎的场景初始化、资产尺度与编辑器调试工作流。原默认场景依赖外部模型和贴图，观察相机也不代表场景对象；本轮改为可检查的启动数据和独立场景相机，不引入通用实体系统。

## 默认内容

| 对象 | 参数 |
| --- | --- |
| Cube | 边长 2 米，局部范围 [-1,1]³，Position/Rotation=0，Scale=1 |
| Default Material | 使用现有默认 PBR：白色、Metallic=0、Roughness=0.5、Opaque，不绑定贴图 |
| Directional Light | 白色，Intensity=1，Rotation=(-54.73561°,45°,0°)，传播方向约 normalize(-1,-2,-1)，投射阴影 |
| Camera | Position=(6,4,6) 米，Rotation=(-25.2394°,45°,0°)，垂直 FOV=50°，near=0.1 米、far=1000 米 |

项目约定 1 单位=1 米，Y 向上、右手坐标，相机和方向光局部前方为 -Z。无参数启动选中 Cube，进入编辑器视图。环境背景仍属于渲染设置，默认关闭反射地面；显式材质、材质实例、透明、实例网格和 Face Shadow 演示保留地面。旧默认模型及其六份模型/材质/贴图资源已移除。

## 加载与所有权

[startup.json](../../assets/scenes/startup.json) 是唯一默认数据来源。[CMake](../../CMakeLists.txt) 在配置时读取它，生成 `StartupSceneEmbedded.h`，修改 JSON 会触发重新配置。运行时 [StartupScene.cpp](../../src/Core/StartupScene.cpp) 优先读取相对工作目录下的 `assets/scenes/startup.json`；文件缺失或校验失败时记录原因，使用内嵌数据。运行时修改文件不会改变内嵌副本，重新构建才更新内嵌数据。

解析器只在实现文件内使用固定版本 nlohmann/json v3.12.0；公开接口返回纯 CPU `StartupSceneDefinition`。校验版本、必要字段、类型、非空名字、有限向量/数值、正边长/Scale、光参数及相机 FOV/near/far，先构造候选，成功后替换结果；失败不会发布部分定义。此 schema 仅支持一个 cube、一个 directionalLight、一个 camera 和 selectedObject，并非任意场景序列化。

[Application::CreateStartupScene](../../src/Core/Application.cpp) 在主循环前实例化定义。Application 拥有 EditableModel、EditableLight、EditableCamera 和选择状态；Renderer 拥有 Mesh/Material/Light 渲染代理及 GPU 资源，场景记录通过句柄引用资源。相机只有 CPU 变换与镜头参数，不创建 GPU 对象。初始化失败或抛出异常时，既有 Shutdown 在 GL 上下文销毁前注销模型 Primitive、清理记录并销毁 Renderer，回收本次所有渲染资源；当前不支持主循环中的启动场景替换，因此没有引入热加载事务或额外 Light 移除接口。

## 正方体几何

[CreateCubeGeometry](../../src/Assets/Geometry/CubeGeometry.h) 生成 24 个渲染顶点和 36 个索引。8 个几何角点在不同面上拆开，以保留硬边法线、独立 UV 和切线。每面定义单位法线 N、切线 T 和 B=cross(N,T)，位置为 `N*h + T*u + B*v`，h=edgeLength/2，u/v∈{-h,h}。UV 为 [0,1]，Tangent.w=1；三角形按外向 CCW 绕序提交。

CPU 上传 Position、Normal、TexCoord、Tangent 与索引，GPU 复用现有标准/Instanced/Tessellation 路径。原点中心包围球半径为 √3 米；根变换默认恒等，不再施加额外单位补偿。默认材质不采样法线/位移贴图，显式命令行贴图参数仍可用于材质调试。本轮没有修改法线贴图的采样算法。

## 方向光与阴影

[EditableLight.cpp](../../src/Editor/EditableLight.cpp) 将 Rotation 变换后的局部 -Z 归一化，写入代理的世界方向；位置不参与方向光照，不暴露 Range 或锥角。每帧不再把方向改为朝向场景中心。CPU 上传仍将方向以 W=0 变换到观察空间，GPU 使用反方向作为表面到光源的 L，不计算距离衰减。

[FitDirectionalShadow](../../src/Core/DirectionalShadow.h) 用已有场景世界包围球拟合单张 2D 正交阴影图：半径添加经模型缩放后的位移裕量，再留 5% 边界；投影观察点沿负传播方向移动，选择不平行的 up，XY 覆盖 [-r,r]，深度覆盖包围球前后端。只改变 CPU 矩阵，复用现有 ShadowPass、2048² 深度资源和 PCF，不新增阴影资源。

PBR/Toon 共用 [pbr.frag](../../assets/shaders/pbr/pbr.frag) 的阴影比较函数。方向光在投影范围外返回可见度 1，聚光灯沿用原越界遮蔽约定；反射地面由 CPU 设置 `shadowIsDirectional` uniform。范围内仍执行九次 PCF 深度比较。当前拟合整个场景的保守球，远距离分段会扩大投影并降低有效精度；不实现 CSM、稳定 texel snapping、LiSPSM 或点光阴影。

## 场景相机与编辑器操作

[EditableCamera](../../src/Editor/EditableCamera.h) 保存会话内稳定 CameraId、名称、Position/Rotation、垂直 FOV 与裁剪面，Scale 固定为 1。`View=inverse(T*R)`，透视矩阵使用实际视口宽高比。初始 Rotation 对准原点，之后移动/旋转不会被自动 LookAt 覆盖。

现有 Camera 保持编辑器 Orbit/Pan/Dolly 功能，两者互不修改。场景相机视图使用自身 near/far；编辑器视图继续自动拟合裁剪范围。相机预览参与编辑器裁剪范围，避免图标被裁掉，但不参与模型包围球、默认聚焦或灯光拟合。GPU 每帧只消费选定视图的矩阵与观察点。

Scene 中选择 Camera，Inspector 可编辑位置、旋转、FOV、near/far，并切换视图。相机支持移动/旋转 Gizmo，R 不会缩放相机；在自身视图不绘制自身工具。编辑器以 ImGui 叠加绘制图标和 1 米长度的视锥预览，按投影位置拾取图标，L 同时控制相机预览显示。预览沿用覆盖式编辑器工具，不以场景深度判断真实遮挡。

- 小键盘 `0`：场景相机视图与编辑器视图切换；无小键盘可用 Inspector 按钮。
- Alt+左/中/右拖动及滚轮：在编辑器视图导航；场景相机视图保持固定。
- F：返回编辑器视图并聚焦选中模型或相机；导入成功同样返回编辑器视图执行一次聚焦。
- 选择 Directional Light：Inspector 的 Rotation 或旋转 Gizmo 修改方向；Ctrl+左拖动直接旋转主方向光。

## 源码参考及适配边界

以下文件均已实际查阅，固定提交用于复核：

- Blender 4.5.0，`8cb6b388974a817afedf1317ce26f0c75aa5f181`：[wm_files.cc](https://github.com/blender/blender/blob/8cb6b388974a817afedf1317ce26f0c75aa5f181/source/blender/windowmanager/intern/wm_files.cc#L1421)、[datafiles/CMakeLists.txt](https://github.com/blender/blender/blob/8cb6b388974a817afedf1317ce26f0c75aa5f181/source/blender/editors/datafiles/CMakeLists.txt#L979)。参考外部 startup 文件优先、内嵌数据回退；本项目采用有限 JSON schema，没有复制 .blend、用户启动目录或 Blender ID 系统。
- OGRE-Next，`475f783d76f21b530e718aab4a548cdd9a222e50`：[OgrePrefabFactory.cpp](https://github.com/OGRECave/ogre-next/blob/475f783d76f21b530e718aab4a548cdd9a222e50/OgreMain/src/OgrePrefabFactory.cpp)。参考 24 顶点/36 索引与面法线、UV；本项目自行构造切线并使用真实 √3 包围半径，不沿用其尺寸常量或资源管理架构。
- Filament，`3630bd4e7348fdc270545acc7dc4bb66d9e43aa8`：[ShadowMap.cpp](https://github.com/google/filament/blob/3630bd4e7348fdc270545acc7dc4bb66d9e43aa8/filament/src/ShadowMap.cpp#L387)。参考灯光空间拟合与方向光投影，适配为现有包围球和单张正交阴影，不复制其复杂投影优化。
- Canavar，`230b44d4feb3592406f78f6aed47ea54cbdb93bd`：[ImGuiWidget.cpp](https://github.com/berkbavas/CanavarGraphicsEngine/blob/230b44d4feb3592406f78f6aed47ea54cbdb93bd/Canavar/Engine/Source/Canavar/Engine/Util/ImGuiWidget.cpp#L774)。参考相机 near/far/FOV 与 active camera 编辑入口，本项目保留独立编辑器相机。

## 验证与待验收

构建与链接通过，全部 16 项测试通过；[StartupSceneTests](../../tests/Core/StartupSceneTests.cpp) 覆盖立方体角点、绕序、切线、定义校验与回退、相机刚体逆矩阵和裁剪面、互斥选择、编辑器导航独立性与阴影包围范围。默认、Material Lab、Material Instance Lab、透明、256 实例和真实 Lumine Face Shadow 场景均完成短时启动，进入实际 Forward 绘制，没有明显初始化、Shader 编译/链接或管线错误。额外运行检查确认启动 JSON 缺失/损坏时使用内嵌回退，关闭方向光阴影时管线仍正常执行；测试后恢复原始 JSON。未新增 RenderDoc 基准。

在可执行文件目录运行 `MaiX_Renderer.exe`，检查 Scene 的 Cube、Directional Light、Camera；核对 Cube 恒等 Transform 和无贴图默认材质；旋转方向光观察照明及阴影；选择相机移动/旋转并修改镜头参数，切换两种视图并确认导航不会改动场景相机；导入 FBX 检查尺度与聚焦；运行显式测试场景检查原有材质、透明、实例与反射功能。画面和交互：**待用户验收**。

本轮不实现通用场景保存/加载、Save Startup Scene、用户配置目录、多相机管理、父子层级、ECS、Blender Workbench、网格地板或新增光照系统。
