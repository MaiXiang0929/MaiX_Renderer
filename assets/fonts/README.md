# 编辑器中文字体

- 字体：Noto Sans SC（TrueType，使用默认字重）。
- 来源：Google Fonts，提交 `bd8f81ddb5c74d5c8897b36ad88b440266245103`。
- 原始文件：[NotoSansSC[wght].ttf](https://github.com/google/fonts/blob/bd8f81ddb5c74d5c8897b36ad88b440266245103/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf)，未修改字节，部署名为 `NotoSansSC.ttf`。
- SHA-256：`a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。
- 启动时的完整性检查：17,772,300 字节，FNV-1a 64 位 `2adecf5b9049c2ad`（检测意外损坏，不用于安全认证）。更换字体时同步更新 `EditorFonts.cpp` 中的固定值及本文来源记录。
- 许可证：[SIL Open Font License 1.1](OFL.txt)，随字体一起部署。

`EditorFonts` 在首帧前将中文、全角标点和相关 CJK 字形合并到 ImGui 默认字体；英文字符沿用默认字体。字体范围保留静态生命周期，字体数据由 ImGui Atlas 持有；GPU 图集由现有 OpenGL 后端上传和释放。使用现有 `assets/` 相对路径约定，从仓库根目录或包含已复制 `assets/` 的可执行文件目录启动。

字体并非涵盖所有 Unicode 字符；本轮不支持 BMP 之外的扩展汉字，也不加入动态字体加载或界面汉化。
