# MCATool - Minecraft MCA 文件查看器

**版本**：2.0.0  
**发布日期**：2026-02-25

---

## 🎮 快速开始

1. 双击运行 **mcatool.exe**
2. 点击菜单 **File -> Open Test Cases** 加载测试文件
3. 在左侧 **Chunk List** 中选择要查看的 Chunk
4. 在 **Chunk Details** 中点击 Section 查看详细信息
5. 在 **Section Viewer** 中切换 2D/3D 视图模式

---

## ✨ 主要功能

### MCA 文件解析
- ✅ 支持 Minecraft 1.2 - 最新版本
- ✅ 完整的 NBT 数据解析
- ✅ 方块和生物群系数据提取

### 图形化界面
- ✅ Region 和 Chunk 信息展示
- ✅ Section 2D 网格视图
- ✅ **Section 3D 立方体视图**（新功能）
- ✅ 方块统计分析

### 3D 立方体视图特性
- 🎨 完整的 16x16x16 方块渲染
- 🖼️ 纹理映射（所有六个面）
- 🖱️ 交互式相机控制
  - 鼠标左键拖拽：旋转视角
  - 鼠标滚轮：缩放距离
- ⚡ 性能优化：完全包裹剔除

---

## 🎯 操作说明

### 菜单栏
- **File -> Open Test Cases** (Ctrl+O)：加载测试案例
- **File -> Open MCA Folder** (Ctrl+Shift+O)：打开自定义文件夹
- **View**：切换各个面板的显示/隐藏

### Section Viewer 控制
- 点击 **"2D Grid"** / **"3D Cube"** 切换视图模式
- **3D 视图中**：
  - 🖱️ 鼠标左键拖拽：旋转视角
  - 🖱️ 鼠标滚轮：缩放距离
  - ☑️ 勾选 "Only show non-air blocks"：隐藏空气方块
  - 🔄 点击 "Reset Camera"：重置相机位置

---

## 📁 文件说明

```
MCATool_Release/
├── mcatool.exe          # 主程序
├── zlib1.dll            # 压缩库（必需）
├── texture/             # 方块纹理文件夹
├── test_mca_files/      # 测试用的 MCA 文件
├── ids.json             # 旧版本方块 ID 映射表
├── 使用说明.txt         # 详细使用说明
└── README.md            # 本文件
```

---

## 🎨 纹理说明

程序会自动从 `texture/` 目录加载方块纹理。

- **文件格式**：PNG
- **命名规则**：方块名称.png（如 `stone.png`）
- **如果纹理不存在**：会使用默认颜色

你可以添加更多纹理文件到 `texture/` 目录。

---

## 💻 系统要求

- **操作系统**：Windows 10 或更高版本
- **显卡**：支持 OpenGL 3.3 或更高版本
- **内存**：建议 4GB 或以上

---

## ❓ 常见问题

### Q: 程序无法启动？
**A**: 确保 `zlib1.dll` 在同一目录下，并且系统支持 OpenGL 3.3

### Q: 3D 视图显示异常？
**A**: 更新显卡驱动，或切换到 2D 视图使用

### Q: 如何加载自己的地图文件？
**A**: 点击 **File -> Open MCA Folder**，选择 Minecraft 存档的 `region` 文件夹

### Q: 纹理不显示？
**A**: 检查 `texture/` 目录是否存在，以及纹理文件命名是否正确

---

## 📝 更新日志

### v2.0.0 (2026-02-25)
- ✅ 新增 Section Viewer 3D 立方体视图
- ✅ 实现交互式相机控制系统
- ✅ 添加纹理映射支持（所有六个面）
- ✅ 性能优化：完全包裹剔除
- ✅ 菜单优化：添加"Open Test Cases"快捷入口

### v1.0.0 (2026-02-24)
- ✅ 基础 GUI 界面实现
- ✅ Region 和 Chunk 信息展示
- ✅ Section 2D 网格视图
- ✅ 方块统计功能

---

## 📄 许可证

MIT License

---

## 🙏 感谢使用

感谢使用 MCATool！如有问题或建议，欢迎反馈。

---

**开发团队**：Aone Copilot  
**项目主页**：https://copilot.code.alibaba-inc.com
