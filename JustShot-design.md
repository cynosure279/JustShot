# JustShot 技术方案
> 基于 JustShot/JustShot.md 的具体实现设计

---

## 1. 构建系统

### 1.1 项目元信息

| 项目 | 值 |
|------|-----|
| 包名 | `justshot` |
| 版本 | `0.1.0` |
| 许可证 | GPL-2.0+ |
| 最低依赖 | GLib ≥ 2.80, GTK ≥ 4.14, libadwaita ≥ 1.6, libjustcapture (subproject) |

### 1.2 Meson 结构

```meson
project('justshot', 'c',
  version: '0.1.0',
  meson_version: '>= 1.3.0',
  license: 'GPL-2.0+',
  default_options: ['warning_level=2', 'c_std=c17'])

add_project_arguments('-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_80',
                      '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_80',
                      '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_14',
                      '-DG_LOG_DOMAIN="JustShot"',
                      language: 'c')

# 依赖：libjustcapture 作为 subproject
libjustcapture_dep = dependency('justcapture', required: false, version: '>= 0.1.0')
if not libjustcapture_dep.found()
  libjustcapture_dep = subproject('libjustcapture').get_variable('libjustcapture_dep')
endif

gtk_dep = dependency('gtk4', version: '>= 4.14')
adw_dep = dependency('libadwaita-1', version: '>= 1.6')

subdir('src/')
subdir('plugins/')
subdir('tests/')
```

### 1.3 二进制目标

```meson
# src/meson.build

justshot_sources = files(
  'main.c',
  'application.c',
  'capture-controller.c',
  'document.c',
  'renderer.c',
  'export.c',
  'ui/window.c',
  'ui/preview.c',
  'ui/toolbar.c',
  'ui/clipboard.c',
  'cli/cli.c',
)

executable('justshot', justshot_sources,
  dependencies: [libjustcapture_dep, gtk_dep, adw_dep],
  install: true,
  gui_app: true)

# GSettings schema
install_data('org.just.JustShot.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')

# Desktop file
install_data('justshot.desktop',
  install_dir: get_option('datadir') / 'applications')
```

---

## 2. 目录结构

```
justshot/
├── meson.build
├── src/
│   ├── meson.build
│   ├── main.c                    # 入口：GApplication 初始化
│   ├── application.c / .h        # JustShotApplication (GtkApplication)
│   ├── capture-controller.c / .h # 核心截图控制器
│   ├── document.c / .h           # ImageDocument（非破坏性编辑）
│   ├── renderer.c / .h           # cairo 渲染器
│   ├── export.c / .h             # PNG 导出（GdkPixbuf）
│   ├── ui/
│   │   ├── window.c / .h         # 主窗口 (AdwApplicationWindow)
│   │   ├── preview.c / .h        # 预览区域
│   │   ├── toolbar.c / .h        # 编辑工具栏
│   │   └── clipboard.c / .h      # 剪贴板操作
│   └── cli/
│       └── cli.c / .h            # CLI 模式
├── plugins/
│   ├── meson.build
│   └── phosh-quick-setting.c     # Phosh 快捷设置插件
├── tests/
│   ├── meson.build
│   ├── test-document.c
│   ├── test-renderer.c
│   └── test-export.c
├── data/
│   ├── org.just.JustShot.gschema.xml
│   ├── justshot.desktop
│   └── icons/
│       └── ... (应用图标)
└── docs/ (可选)
```

---

## 3. GApplication 与 D-Bus 动作

### 3.1 应用架构

```c
/* application.h */
#define JUST_SHOT_APPLICATION_ID "org.just.JustShot"

typedef struct {
  GtkApplication parent_instance;
  /* 内部状态 */
  CaptureController *controller;
  /* ... */
} JustShotApplication;
```

**启动流程**：

```
main()
  └─ g_application_new("org.just.JustShot", G_APPLICATION_HANDLES_COMMAND_LINE)
       └─ g_application_add_main_option("target", 't', ...)
       └─ g_application_add_main_option("delay", 'd', ...)
       └─ g_application_add_main_option("output", 'o', ...)
       └─ g_application_add_main_option("clipboard", 'c', ...)
       └─ g_application_add_main_option("interactive", 'i', ...)
       └─ g_application_run()
            └─ activate → 启动 GUI
            └─ command-line → 解析 CLI 参数
            └─ GAction "capture-default" → 默认截图
            └─ GAction "capture-screen" → 全屏
            └─ GAction "capture-window" → 窗口
            └─ GAction "capture-area" → 区域
            └─ GAction "capture-active-window" → 活动窗口
            └─ GAction "capture-delay" → 延迟截图（带参数）
```

### 3.2 D-Bus Actions

核心设计：**所有截图操作通过 D-Bus Action 触发**，GUI 和 CLI 同一套机制。

```xml
<!-- org.just.JustShot.gschema.xml -->
<schemalist>
  <schema id="org.just.JustShot" path="/org/just/justshot/">
    <key name="default-format" type="s">
      <default>'png'</default>
      <summary>Default output format</summary>
    </key>
    <key name="default-target" type="u">
      <default>1</default>  <!-- SCREEN = 1 -->
      <summary>Default capture target</summary>
    </key>
    <key name="include-pointer" type="b">
      <default>true</default>
      <summary>Include pointer in screenshot</summary>
    </key>
    <key name="save-directory" type="s">
      <default>''</default>
      <summary>Custom save directory (empty = default)</summary>
    </key>
  </schema>
</schemalist>
```

**GAction 注册**：

| Action | Parameter Type | State | 触发效果 |
|--------|---------------|-------|---------|
| `capture-default` | — | — | 按默认设置截图 |
| `capture-screen` | — | — | 全屏截图 |
| `capture-window` | — | — | 窗口截图 |
| `capture-area` | — | — | 区域截图 |
| `capture-active-window` | — | — | 活动窗口截图 |
| `capture-delay` | `u` | — | 指定秒数延迟后截图 |
| `set-target` | — | `u` | 设置默认目标 |

---

## 4. CaptureController 核心

### 4.1 职责

```c
/* capture-controller.h */

typedef struct _CaptureController CaptureController;

CaptureController *capture_controller_new (GApplication *app);

/* 异步截图入口 */
void capture_controller_capture_async (
    CaptureController        *self,
    JustCaptureScreenshotTarget target,
    guint                     delay_seconds,
    gboolean                  interactive,
    GCancellable             *cancellable,
    GAsyncReadyCallback       callback,
    gpointer                  user_data);
gboolean capture_controller_capture_finish (
    CaptureController *self,
    GAsyncResult      *result,
    GError           **error);
```

**内部流程**：

```
capture_controller_capture_async()
  ├─ 1. 探测能力（如果尚未探测）
  │     ├─ capabilities_query_async()
  │     └─ target 不在可用列表中 → 回退到 interactive
  ├─ 2. 延迟等待（如果 delay > 0）
  │     └─ g_timeout_add_seconds() → 继续
  ├─ 3. 发起 Portal 截图请求
  │     └─ just_capture_screenshot_request_async()
  ├─ 4. 得到 URI
  ├─ 5. 解码 URI → 临时文件路径
  │     └─ document: 协议 → 读 Documents Portal
  │     └─ file: 协议 → 直接使用
  ├─ 6. 加载图片到 ImageDocument
  │     └─ document_load_from_file()
  ├─ 7. 非交互模式 → 直接保存到输出路径
  ├─ 8. 交互模式 → 打开预览窗口
  └─ 9. 返回结果（成功/失败）
```

### 4.2 状态管理

```c
typedef enum {
  CAPTURE_STATE_IDLE,
  CAPTURE_STATE_PROBING,       /* 正在探测能力 */
  CAPTURE_STATE_WAITING_DELAY, /* 延迟等待中 */
  CAPTURE_STATE_REQUESTING,    /* 正在请求 Portal */
  CAPTURE_STATE_DECODING,      /* 正在解码 URI */
  CAPTURE_STATE_EDITING,       /* 用户正在编辑 */
  CAPTURE_STATE_SAVING,        /* 正在保存/导出 */
  CAPTURE_STATE_CANCELLED,     /* 已取消（非错误） */
  CAPTURE_STATE_ERROR,         /* 错误状态 */
} CaptureState;
```

---

## 5. ImageDocument 非破坏性编辑模型（md §6）

### 5.1 设计原则

- **非破坏性**：编辑操作不修改原始像素数据，而是记录操作栈
- **撤销/重做**：操作栈支持 undo/redo
- **延迟渲染**：仅在需要显示或导出时渲染最终结果
- **操作类型**：裁剪、旋转、翻转、标注（矩形、箭头、文字、自由画笔、模糊/马赛克）、颜色调整

### 5.2 数据结构

```c
/* document.h */

typedef enum {
  DOCUMENT_OP_NONE,
  DOCUMENT_OP_CROP,
  DOCUMENT_OP_ROTATE,
  DOCUMENT_OP_FLIP,               /* 水平或垂直翻转 */
  DOCUMENT_OP_DRAW_RECTANGLE,     /* 矩形标注 */
  DOCUMENT_OP_DRAW_ELLIPSE,       /* 椭圆标注 */
  DOCUMENT_OP_DRAW_ARROW,         /* 箭头标注 */
  DOCUMENT_OP_DRAW_TEXT,          /* 文字标注 */
  DOCUMENT_OP_DRAW_FREEHAND,      /* 自由画笔 */
  DOCUMENT_OP_BLUR,               /* 模糊/马赛克 */
  DOCUMENT_OP_ADJUST_COLOR,       /* 颜色调整 */
} DocumentOpType;

typedef struct {
  DocumentOpType type;
  /* 操作参数（随类型不同，存为 GVariant 或专用 union） */
  GVariant *params;
  /* 时间戳用于排序 */
  gint64    timestamp_us;
} DocumentOperation;

typedef struct _ImageDocument {
  /* 原始图像（不变） */
  cairo_surface_t *original_surface;
  gchar           *original_file_path;
  gint             original_width, original_height;

  /* 操作栈 */
  GList           *ops;        /* GList<DocumentOperation*> */
  GList           *saved_pos;  /* 保存时的位置，用于 dirty 检测 */
  GList           *current;    /* 当前操作位置（撤销点） */

  /* 缓存 */
  cairo_surface_t *cached_result; /* 最后渲染的缓存 */
  gboolean         cache_dirty;

  /* 信号 */
  guint            changed_id;
  /* ... */
} ImageDocument;
```

### 5.3 关键 API

```c
/* 创建/加载 */
ImageDocument *image_document_new_from_file (const gchar *file_path, GError **error);
ImageDocument *image_document_new_from_data (gconstpointer data, gsize len, GError **error);

/* 操作栈 */
void image_document_push_op (ImageDocument *doc, DocumentOpType type, GVariant *params);
void image_document_undo (ImageDocument *doc);
void image_document_redo (ImageDocument *doc);
gboolean image_document_can_undo (ImageDocument *doc);
gboolean image_document_can_redo (ImageDocument *doc);

/* 清除所有操作 */
void image_document_reset (ImageDocument *doc);

/* 渲染 */
cairo_surface_t *image_document_render (ImageDocument *doc, GError **error);

/* 导出 */
void image_document_export_png_async (
    ImageDocument   *doc,
    const gchar     *output_path,
    GCancellable    *cancellable,
    GAsyncReadyCallback callback,
    gpointer         user_data);
gboolean image_document_export_png_finish (
    ImageDocument *doc,
    GAsyncResult  *result,
    GError       **error);

/* 保存/脏 */
gboolean image_document_is_dirty (ImageDocument *doc);
void image_document_mark_saved (ImageDocument *doc);

/* 尺吋 */
gint image_document_get_width (ImageDocument *doc);
gint image_document_get_height (ImageDocument *doc);
```

### 5.4 操作参数

```c
/* 裁剪操作参数 */
typedef struct {
  gint x, y, width, height;
} CropParams;

/* 旋转参数 */
typedef enum { ROTATE_90, ROTATE_180, ROTATE_270 } RotateAngle;

/* 翻转参数 */
typedef enum { FLIP_HORIZONTAL, FLIP_VERTICAL } FlipDirection;

/* 矩形参数 */
typedef struct {
  gdouble x1, y1, x2, y2;        /* 坐标（归一化 0.0-1.0 或像素） */
  gdouble stroke_width;
  gdouble stroke_r, stroke_g, stroke_b, stroke_a;
  gdouble fill_r, fill_g, fill_b, fill_a;  /* alpha=0 表示不填充 */
} RectangleParams;

/* 文字参数 */
typedef struct {
  gdouble x, y;
  gchar  *text;
  gchar  *font_family;
  gdouble font_size;
  gdouble color_r, color_g, color_b, color_a;
} TextParams;

/* 模糊参数 */
typedef struct {
  gdouble x, y, width, height;
  gdouble radius;  /* 高斯模糊半径 */
} BlurParams;

/* 颜色调整参数 */
typedef struct {
  gdouble brightness;  /* -1.0 ~ 1.0 */
  gdouble contrast;    /* 0.0 ~ 2.0 */
  gdouble saturation;  /* 0.0 ~ 2.0 */
} ColorAdjustParams;
```

---

## 6. Renderer（cairo 渲染器）

```c
/* renderer.h */

/* 渲染 ImageDocument 操作栈到目标 surface */
cairo_surface_t *renderer_render (
    ImageDocument *doc,
    GError       **error);

/* 增量渲染：从指定操作开始渲染（用于撤销/重做优化） */
cairo_surface_t *renderer_render_from (
    ImageDocument *doc,
    GList         *start_op,
    GError       **error);
```

**实现要点**：
- 从原始图像 `cairo_image_surface_create_from_png` 或 `cairo_image_surface_create_for_data` 开始
- 逐个执行操作栈中的操作（cairo 绘制）
- 裁剪：`cairo_surface_create_for_rectangle` + 重新绘制
- 旋转：`cairo_translate` + `cairo_rotate` + 偏移计算
- 标注：标准 cairo 绘制（`cairo_set_source_rgba`、`cairo_move_to`、`cairo_line_to`、`cairo_show_text` 等）
- 模糊：`cairo_surface` 采样 → 高斯模糊算法 → 回写
- 颜色调整：遍历像素数据，逐个像素调整
- 缓存：渲染完成后缓存到 `cached_result`，操作栈变化时标记 `cache_dirty`

---

## 7. Export（PNG 导出）

```c
/* export.h */

/* 使用 GdkPixbuf 保存为 PNG */
void export_to_png_async (
    const gchar     *source_path,     /* 或 cairo_surface_t */
    const gchar     *output_path,
    GCancellable    *cancellable,
    GAsyncReadyCallback callback,
    gpointer         user_data);
gboolean export_to_png_finish (
    GAsyncResult *result,
    GError      **error);
```

**实现**：
- 将 `cairo_surface_t` 转换为 `GdkPixbuf`（`gdk_pixbuf_get_from_surface`）
- 使用 `gdk_pixbuf_savev_async` 保存为 PNG
- 异步写入，不阻塞 UI
- 导出路径：`just_capture_output_path_make(JUST_CAPTURE_OUTPUT_KIND_SCREENSHOT, filename)`

---

## 8. UI 层

### 8.1 主窗口

```c
/* ui/window.h */

typedef struct _JustShotWindow JustShotWindow;

JustShotWindow *just_shot_window_new (JustShotApplication *app);
void just_shot_window_show_preview (JustShotWindow *win, ImageDocument *doc);
void just_shot_window_set_state (JustShotWindow *win, CaptureState state);
```

**设计**（md §5）：

```
┌──────────────────────────────────────────────┐
│ [Capture—] [Screen] [Window] [Area] [Active] │  ← 顶部工具栏
│ [Delay: 3s ▼]                                │
├──────────────────────────────────────────────┤
│                                              │
│              Preview Area                    │  ← 截图预览
│          (缩放/平移/全屏)                     │
│                                              │
├──────────────────────────────────────────────┤
│ [Crop] [Rotate] [Flip] [Undo] [Redo] [Blur] │  ← 编辑工具栏
│ [Rectangle] [Arrow] [Text] [Pen]             │
│ [Export] [Save As...] [Copy] [Share] [×]     │  ← 操作栏
└──────────────────────────────────────────────┘
```

### 8.2 预览区域

- 使用 `GtkDrawingArea`，自定义 `snapshot` 回调渲染 `cairo_surface_t`
- 支持缩放（`GtkGestureZoom` + 滚轮）和平移（`GtkGesturePan`）
- 全屏模式（`GtkWindow:fullscreened`）
- 实时显示 ImageDocument 渲染结果

### 8.3 编辑工具栏

- 操作按钮 → 调用 `image_document_push_op()` → 触发 `changed` 信号 → 刷新预览
- 撤销/重做 → `image_document_undo()/redo()` → 刷新预览
- 裁剪模式：鼠标拖拽选择区域 → 确认 → 推入 CropOp
- 标注模式：鼠标拖拽绘制 → 推入对应 Op
- 模糊模式：鼠标拖拽选择区域 → 高斯模糊 Op

### 8.4 剪贴板

```c
/* ui/clipboard.h */

void clipboard_copy_surface (cairo_surface_t *surface);
gboolean clipboard_has_image (void);
```

使用 `GdkClipboard`，设置 `GdkContentProvider`：
- 复制 PNG 数据到剪贴板（`gdk_content_provider_new_typed`）
- 接收方可通过 `gdk_clipboard_read_async` 读取

---

## 9. CLI 模式

```c
/* cli/cli.h */

typedef struct {
  gint    target;          /* 截图目标，0=default */
  guint   delay_seconds;   /* 延迟秒数 */
  gchar  *output_path;     /* 输出路径，NULL=默认 */
  gboolean clipboard;      /* 是否复制到剪贴板 */
  gboolean interactive;    /* 交互模式 */
  gboolean quiet;          /* 静默模式 */
  gboolean version;        /* 显示版本 */
} CliOptions;

gboolean cli_parse_args (GApplication *app, int argc, char *argv[], CliOptions *opts);
void cli_execute (GApplication *app, CliOptions *opts);
```

**CLI 使用示例**：

```bash
# 默认截图（设置中的默认目标）
justshot

# 全屏截图
justshot --target=screen

# 区域截图
justshot --target=area

# 3秒延迟后截图到指定文件
justshot --target=window --delay=3 --output=~/window.png

# 截图到剪贴板
justshot --target=active-window --clipboard

# 交互模式（显示编辑器）
justshot --interactive

# 静默模式（非交互，不打开编辑器）
justshot --target=screen --quiet
```

**实现**：CLI 模式通过 GApplication 的 `handle-local-options` 或 `command-line` 信号处理。如果指定了 `--quiet`，则截取后直接保存，不打开 GUI 窗口。如果指定了 `--interactive`，则打开 GUI 编辑窗口。

---

## 10. Phosh 快捷设置插件

### 10.1 设计原则（md §8）

- 插件**不**做任何 Portal 操作
- 插件**不**保存文件
- 插件**不**进行图片编辑
- 插件**不**直接调用 libjustcapture
- 插件仅通过 GDBusActionGroup 触发 JustShot 的 D-Bus 动作

### 10.2 实现

```c
/* plugins/phosh-quick-setting.c */

#include <phosh-plugin.h>

/* 快速设置结构 */
typedef struct {
  PhoshQuickSetting parent;
  GDBusActionGroup *action_group;
  /* ... */
} JustShotQuickSetting;

/* 实例化时连接 JustShot 的 D-Bus ActionGroup */
static void
just_shot_quick_setting_init (JustShotQuickSetting *self)
{
  /* 获取 org.just.JustShot 的 D-Bus ActionGroup */
  self->action_group = g_dbus_action_group_get (
      G_BUS_TYPE_SESSION, "org.just.JustShot", "/org/just/justshot");

  /* 设置 quick setting 属性 */
  phosh_quick_setting_set_title (PHOSH_QUICK_SETTING (self), "Screenshot");
  phosh_quick_setting_set_icon_name (PHOSH_QUICK_SETTING (self), "camera-photo-symbolic");

  /* 点击时触发默认截图动作 */
  g_signal_connect_swapped (self, "clicked",
      G_CALLBACK (activate_action), self->action_group);
}

static void
activate_action (GDBusActionGroup *actions)
{
  /* 激活 "capture-default" action */
  g_dbus_action_group_activate_action (actions, "capture-default", NULL);
}
```

**构建**：

```meson
# plugins/meson.build
phosh_dep = dependency('phosh-plugins', required: get_option('phosh-plugin'))

if phosh_dep.found()
  shared_library('justshot-quick-setting',
    'phosh-quick-setting.c',
    dependencies: [phosh_dep, glib_dep, gio_dep],
    install: true,
    install_dir: phosh_dep.get_variable('plugindir'))
endif
```

---

## 11. 保存与磁盘操作

### 11.1 保存流程

```
capture_controller_capture_finish()
  └─ 非交互模式：
       ├─ 1. just_capture_output_path_get_screenshots_dir()
       ├─ 2. just_capture_filename_make_screenshot(timestamp)
       ├─ 3. just_capture_filename_make_unique(dir, basename, ".png")
       ├─ 4. 临时文件 → GdkPixbuf → PNG 编码 → 写入临时路径
       ├─ 5. g_file_set_contents() 或 g_file_replace()
       └─ 6. 通知文件管理器（可选，portal 或 g_file_notify）
  └─ 交互模式：
       └─ 用户点击"保存"或"另存为"时执行上述流程
```

### 11.2 分享/导出

- 通过 `org.freedesktop.portal.OpenURI` 或文件管理器打开输出目录
- 未来可集成 `org.freedesktop.portal.Share`（md 可选）

---

## 12. 依赖树

```
justshot (GUI可执行文件 + CLI)
├── libjustcapture  (Portal 封装、能力探测、输出路径、文件名)
├── GTK 4           (UI 框架：GtkApplication, GtkWindow, GtkDrawingArea, GtkWidget)
├── libadwaita      (AdwApplicationWindow, AdwHeaderBar, AdwToolbarView)
├── GLib / GIO     (GApplication, GTask, GAction, GSettings, GDBusActionGroup)
├── cairo          (图像渲染：cairo_surface_t, cairo_t)
├── GdkPixbuf      (PNG 编码/解码、剪贴板提供)
└── Gdk            (GdkClipboard, GdkContentProvider)
```

---

## 13. 与 md 的合规性检查

| md § | 要求 | 实现 | 状态 |
|------|------|------|------|
| §1 | 截图工具 | JustShot 二进制 + CLI | ✅ |
| §2 | 不持久化运行 | GApplication 按需启动 | ✅ |
| §3 | libjustcapture 提供 Portal URI | CaptureController 使用 libjustcapture | ✅ |
| §4 | GUI 和 CLI 共享核心 | GApplication + D-Bus actions | ✅ |
| §5 | Phosh 插件不处理 Portal | 仅 GDBusActionGroup | ✅ |
| §6 | 编辑器 | ImageDocument 非破坏性编辑 | ✅ |
| §7 | 默认 PNG | 导出 PNG | ✅ |
| §8 | 延迟截图 | capture_controller 支持 delay | ✅ |
| §9 | 剪贴板 | clipboard_copy_surface | ✅ |
| §10 | 不依赖 PipeWire/GStreamer | 构建系统不链接 | ✅ |
| §11 | libjustcapture 不依赖 GTK | 独立库 | ✅ |
| §12 | 无公共设置 | 各自 GSettings schema | ✅ |

---

## 14. 里程碑与验收

### MVP 验收标准

| # | 验收项 | 对应模块 | 验证方式 |
|---|--------|---------|---------|
| 1 | `justshot --target=screen` 能截取全屏保存为 PNG | capture-controller + export | 命令行运行，检查输出文件 |
| 2 | `justshot --target=area --interactive` 能区域截图并打开编辑器 | capture-controller + ui/window | 点击区域，确认编辑器打开 |
| 3 | 编辑器中可裁剪图片并保存 | ui/preview + document + export | 裁剪操作 → 保存 → 检查结果 |
| 4 | 编辑器中可添加箭头标注 | ui/preview + document | 绘制箭头 → 预览显示 |
| 5 | 撤销/重做操作 | document | 多次操作 → Ctrl+Z → 状态正确 |
| 6 | `justshot --clipboard` 截图到剪贴板 | clipboard | 截图后粘贴到其他应用 |
| 7 | `justshot --delay=5` 延迟截图 | capture-controller | 5 秒后截图 |
| 8 | Phosh 插件显示"截图"按钮 | plugins/phosh-quick-setting | 安装后 Phosh 面板显示按钮 |
| 9 | 点击 Phosh 按钮触发默认截图 | plugins/phosh-quick-setting + application | 插件点击 → 截图保存 |
| 10 | 用户取消 Portal 对话框无崩溃 | capture-controller + libjustcapture | 取消对话框 → 回到 IDLE |
