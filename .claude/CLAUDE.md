# MaiX_Renderer

一个基于现代 OpenGL (Core Profile) 的 Interactive Computer Graphics 练习项目。

## 参考课程信息
Interactive Computer Graphics.
School of Computing, University of Utah.
Full Playlist:    • Interactive Computer Graphics  
Course website: https://graphics.cs.utah.edu/courses/cs6610/spring2021/

## 开发环境要求

- **操作系统**: Windows 10 / 11 (x64)
- **编译器/IDE**: Visual Studio 2022 (或支持 C++17 的编译器)
- **构建工具**: CMake 3.15 或更高版本

## 第三方依赖库说明

本项目将第三方依赖存放在根目录的 `libs/` 文件夹中。若要自行编译，请确保目录结构如下：
```text
libs/
├── glad/
│   ├── include/
│   └── src/glad.c
└── glfw/
    ├── include/
    └── lib-vc2022/glfw3.lib  # 仅支持 VS2022 编译器
```

## projects文件夹说明
projects为课程作业项目的实现步骤及代码的obsidian笔记

## 目前进度。。。
目前已完成以下内容：
Project 1 : Hello World
Project 2 : Transformations
Project 3 : Shading
