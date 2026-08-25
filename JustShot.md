JustShot：自适应 Wayland 截图工具设计草案

1. 定位

JustShot 是一次性截图工具。

核心原则：

«请求截图 → 处理图片 → 保存/编辑/分享 → 任务结束。»

不常驻后台，不运行永久 daemon。

支持：

- Phosh
- 其他支持 XDG Portal 的 Wayland 桌面
- 手机与桌面自适应 GTK UI

---

2. 架构

                 JustShot GTK
                     │
               JustShot Core
                     │
       ┌─────────────┼──────────────┐
       │             │              │
    Capture        Editor         Export
       │
 libjustcapture
       │
 Screenshot Portal
       │
   Compositor

Phosh：

JustShot Quick Setting
         │
         ▼
 JustShot Application
         │
         ▼
   JustShot Core

Phosh 插件不实现截图。

---

3. JustShot 自己负责什么

JustShot owns：

- 截图任务生命周期
- 选择 screenshot target
- Portal 返回图片的读取
- 图片 document
- 图像编辑
- 图片编码
- 最终保存
- Clipboard
- 分享
- GTK UI
- Phosh Quick Setting
- JustShot 设置

不负责：

- ScreenCast
- PipeWire
- 视频
- 音频
- GStreamer
- 长生命周期 service

---

4. Capture 流程

用户请求截图
      ↓
读取 libjustcapture capabilities
      ↓
选择 Screen / Window / Area / Active Window
      ↓
libjustcapture.request_screenshot()
      ↓
Portal Result URI
      ↓
JustShot ImageDocument
      ↓
保存 / 编辑 / 分享

Screenshot Portal v3 原生公开 Screen、Window、Area、Active Window targets，因此 UI 必须按实际 capability 动态展示，而不是写死。

---

5. 产品模式

快速截图

点击 Quick Setting
      ↓
Screenshot
      ↓
保存
      ↓
显示预览通知

目标是手机系统截图体验。

完整模式

打开 JustShot：

截图方式
────────────
整个屏幕
窗口
区域
当前窗口

延迟
0 / 3 / 5 秒

[截屏]

---

6. 截图完成后的 UI

手机：

┌──────────────────────┐
│                      │
│      Screenshot      │
│                      │
├──────────────────────┤
│ 编辑   分享   删除   │
└──────────────────────┘

桌面：

┌──────────────┬────────────────────────┐
│ Capture      │                        │
│              │        Preview         │
│ Screen       │                        │
│ Window       │                        │
│ Area         │                        │
│              │                        │
│ Delay        │                        │
│              │ 编辑 / 保存 / 分享     │
└──────────────┴────────────────────────┘

同一套 GTK UI，通过 adaptive layout 重排。

---

7. ImageDocument

截图结果进入：

JustShotImageDocument
├── source image
├── dimensions
├── metadata
├── edit operations
└── dirty state

编辑操作尽量非破坏：

Source
 +
Crop
 +
Arrow
 +
Text
 +
Blur
 ↓
Renderer
 ↓
Export

---

8. 编辑器范围

第一阶段：

- Crop
- Rotate
- Pen
- Arrow
- Rectangle
- Text
- Blur/Mosaic
- Undo
- Redo

不做：

- 图层专业编辑
- RAW
- 色彩工作流
- Photoshop/GIMP 级功能

JustShot 是截图标注器。

---

9. 输出

默认：

~/Pictures/Screenshots/

由 "libjustcapture" 生成目标路径。

JustShot 自己负责：

Portal URI
   ↓
load
   ↓
optional edits
   ↓
encode PNG
   ↓
final file

第一版默认 PNG。

后续：

- JPEG
- WebP

---

10. Clipboard / Share

JustShot 自己负责用户动作：

复制
分享
打开
删除

但平台操作尽量走标准 desktop APIs / Portal。

不要写：

share_to_qq()
share_to_telegram()

这样的应用专用代码。

---

11. Phosh Plugin

提供：

JustShot Quick Setting

普通点击：

[截屏]
  ↓
使用默认模式立即截图

StatusPage：

截屏
──────────
整个屏幕
窗口
区域

延迟    3 秒

[截屏]

Phosh 原生 Quick Setting 支持关联 "PhoshStatusPage"，适合这种附加操作。

插件责任

只负责：

- 显示入口
- 读取 JustShot 状态/设置
- 发起 JustShot action

不负责：

- Portal request
- 保存图片
- 编辑

---

12. Application 激活

建议 JustShot 使用 D-Bus activatable "GApplication"。

提供 actions：

capture-default
capture-screen
capture-window
capture-area
open

因此：

Phosh plugin
CLI
desktop shortcut
keyboard shortcut

都调用同一套 JustShot application action。

不需要 permanent daemon。

---

13. CLI

justshot capture
justshot capture --screen
justshot capture --window
justshot capture --area
justshot capture --delay 3

CLI 应调用同一个 application/core，而不是重新实现截图。

---

14. 代码结构

justshot/
├── src/
│   ├── app/
│   ├── capture/
│   │   └── shot-controller.c
│   ├── image/
│   │   ├── document.c
│   │   ├── editor.c
│   │   └── renderer.c
│   ├── export/
│   │   └── image-exporter.c
│   └── ui/
│
├── phosh/
│   └── justshot-quick-setting/
│
└── data/

依赖：

JustShot
├── GTK/libadwaita
├── libjustcapture
└── image codec libs

---

15. MVP

Phase 1

JustShot
→ libjustcapture
→ Screenshot Portal
→ URI
→ PNG

Phase 2

支持 capability-aware target。

Phase 3

截图后预览。

Phase 4

Phosh Quick Setting。

Phase 5

基础编辑器。

---

16. MVP 验收

1. Phosh 下能可靠截图。
2. Screen/Window/Area 按 Portal capability 显示。
3. 能保存 PNG。
4. 截图后有预览。
5. Quick Setting 可直接触发截图。
6. GTK App 关闭后不存在无意义后台进程。
7. CLI、Quick Setting、GUI 使用同一个核心。
8. 用户取消 Portal 不视作错误。
9. 不依赖 grim。
10. 完全不包含录屏代码。

JustShot 的责任结束于“得到最终图片并交给用户”。