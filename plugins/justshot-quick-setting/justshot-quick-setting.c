/*
 * Copyright (C) 2026 The JustShot Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "justshot-quick-setting.h"
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <time.h>
#define PHOSH_SS_BUS "org.gnome.Shell.Screenshot"
#define PHOSH_SS_PATH "/org/gnome/Shell/Screenshot"
#define PHOSH_SS_IFACE "org.gnome.Shell.Screenshot"
#define NOTIFY_BUS "org.freedesktop.Notifications"
#define NOTIFY_PATH "/org/freedesktop/Notifications"
#define NOTIFY_IFACE "org.freedesktop.Notifications"
enum { ROW_MODE_FULL, ROW_MODE_AREA, ROW_DELAY_0, ROW_DELAY_1, ROW_DELAY_3, ROW_DELAY_10, ROW_SEPARATOR };
typedef struct { gint type; const char *label; } RowDef;
typedef struct { gboolean area_mode; } CaptureData;
static const RowDef rows[] = {
  { ROW_MODE_FULL, N_("Full screen") }, { ROW_MODE_AREA, N_("Area selection") },
  { ROW_SEPARATOR, NULL }, { ROW_DELAY_0, N_("Immediate (0s)") },
  { ROW_DELAY_1, N_("1 second") }, { ROW_DELAY_3, N_("3 seconds") }, { ROW_DELAY_10, N_("10 seconds") },
};
struct _JustshotQuickSetting {
  PhoshQuickSetting parent; PhoshStatusIcon *info; PhoshStatusPage *status_page;
  GtkListBox *list_box; GtkLabel *btn_label; GSettings *settings;
  gboolean area_mode; guint selected_delay; GtkWidget *check_widgets[7];
};
G_DEFINE_TYPE (JustshotQuickSetting, justshot_quick_setting, PHOSH_TYPE_QUICK_SETTING)
static void send_notification (const char *summary, const char *body) {
  GDBusConnection *c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (!c) return; GVariantBuilder b; g_variant_builder_init (&b, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&b, "{sv}", "category", g_variant_new_string ("device"));
  g_dbus_connection_call (c, NOTIFY_BUS, NOTIFY_PATH, NOTIFY_IFACE, "Notify",
    g_variant_new ("(susssasa{sv}i)", "JustShot", 0, "camera-photo-symbolic", summary, body ? body : "", NULL, &b, -1),
    NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL); g_object_unref (c);
}
static char *make_screenshot_filename (void) {
  g_autoptr(GDateTime) ts = g_date_time_new_now_local ();
  return g_date_time_format (ts, "Screenshot_%Y-%m-%d_%H-%M-%S");
}
static char *get_screenshots_dir (void) {
  const char *p = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (!p) p = g_get_home_dir (); return g_build_filename (p, "Screenshots", NULL);
}
static char *make_unique_path (const char *dir, const char *basename) {
  char *path; gint i;
  for (i = 0; i < 1000; i++) {
    if (i == 0) { char *fn = g_strdup_printf ("%s.png", basename); path = g_build_filename (dir, fn, NULL); g_free (fn); }
    else { char *n = g_strdup_printf ("%s_%d.png", basename, i); path = g_build_filename (dir, n, NULL); g_free (n); }
    if (!g_file_test (path, G_FILE_TEST_EXISTS)) return path; g_free (path);
  } return NULL;
}
static void save_and_notify (const char *filename_used) {
  g_autofree char *basename = make_screenshot_filename ();
  g_autofree char *dir = get_screenshots_dir ();
  g_autofree char *unique = NULL; g_autoptr(GFile) src = NULL; g_autoptr(GFile) dst = NULL; g_autoptr(GError) err = NULL;
  g_message ("justshot: save_and_notify filename=%s", filename_used);
  if (g_mkdir_with_parents (dir, 0755) != 0) { send_notification (_("Screenshot Failed"), _("Cannot create dir")); g_unlink (filename_used); return; }
  unique = make_unique_path (dir, basename); if (!unique) { g_unlink (filename_used); return; }
  src = g_file_new_for_path (filename_used); dst = g_file_new_for_path (unique);
  if (g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err)) {
    g_autofree char *sn = g_path_get_basename (unique); g_autofree char *body = g_strdup_printf (_("Saved: %s"), sn);
    g_message ("justshot: saved %s", unique); send_notification (_("Screenshot Saved"), body);
  } else { g_warning ("justshot: copy failed: %s", err->message); send_notification (_("Screenshot Failed"), err->message); }
  g_unlink (filename_used);
}
static void on_screenshot_done (GObject *source, GAsyncResult *res, gpointer user_data) {
  GError *error = NULL; g_autoptr(GVariant) result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), res, &error);
  g_message ("justshot: on_screenshot_done fired");
  if (result == NULL) {
    g_autofree char *msg = g_strdup_printf (_("Screenshot failed: %s"), error ? error->message : _("unknown"));
    g_warning ("justshot: dbus call failed: %s", error ? error->message : "no error");
    send_notification (_("Screenshot Failed"), msg); if (error) g_error_free (error); g_free (user_data); return;
  }
  gboolean success = FALSE; const char *filename_used = NULL;
  g_variant_get (result, "(b&s)", &success, &filename_used);
  g_message ("justshot: result success=%d filename=%s", success, filename_used ? filename_used : "(null)");
  if (success && filename_used && *filename_used) save_and_notify (filename_used);
  else send_notification (_("Screenshot Failed"), _("No screenshot captured"));
  g_free (user_data);
}
static void take_screenshot (GDBusConnection *conn, gboolean area, gint x, gint y, gint w, gint h) {
  g_autofree char *tmp = g_strdup_printf ("/tmp/justshot-%d.png", (gint) getpid ());
  g_message ("justshot: take_screenshot area=%d tmp=%s", area, tmp);
  if (area && w > 0 && h > 0)
    g_dbus_connection_call (conn, PHOSH_SS_BUS, PHOSH_SS_PATH, PHOSH_SS_IFACE, "ScreenshotArea",
      g_variant_new ("(iiiibs)", x, y, w, h, FALSE, tmp), G_VARIANT_TYPE ("(bs)"), G_DBUS_CALL_FLAGS_NONE, 30000, NULL, on_screenshot_done, NULL);
  else
    g_dbus_connection_call (conn, PHOSH_SS_BUS, PHOSH_SS_PATH, PHOSH_SS_IFACE, "Screenshot",
      g_variant_new ("(bbs)", TRUE, FALSE, tmp), G_VARIANT_TYPE ("(bs)"), G_DBUS_CALL_FLAGS_NONE, 15000, NULL, on_screenshot_done, NULL);
}
static void on_select_area_done (GObject *source, GAsyncResult *res, gpointer user_data) {
  CaptureData *data = user_data; GDBusConnection *conn = G_DBUS_CONNECTION (source);
  GError *error = NULL; g_autoptr(GVariant) result = g_dbus_connection_call_finish (conn, res, &error);
  g_message ("justshot: on_select_area_done fired");
  if (result == NULL) { if (error) g_error_free (error); g_free (data); return; }
  gint x, y, w, h; g_variant_get (result, "(iiii)", &x, &y, &w, &h);
  g_message ("justshot: area %d,%d %dx%d", x, y, w, h);
  if (w <= 0 || h <= 0) { g_free (data); return; }
  take_screenshot (conn, TRUE, x, y, w, h);
}
static gboolean do_capture (gpointer user_data) {
  CaptureData *data = user_data; GDBusConnection *conn;
  g_message ("justshot: do_capture area=%d", data ? data->area_mode : -1);
  conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (!conn) { g_warning ("justshot: no session bus"); g_free (data); return G_SOURCE_REMOVE; }
  g_message ("justshot: got bus conn %p", conn);
  if (data->area_mode) {
    g_message ("justshot: calling SelectArea");
    g_dbus_connection_call (conn, PHOSH_SS_BUS, PHOSH_SS_PATH, PHOSH_SS_IFACE, "SelectArea",
      NULL, G_VARIANT_TYPE ("(iiii)"), G_DBUS_CALL_FLAGS_NONE, 60000, NULL, on_select_area_done, data);
  } else {
    g_message ("justshot: calling Screenshot");
    take_screenshot (conn, FALSE, 0, 0, 0, 0); g_free (data);
  }
  return G_SOURCE_REMOVE;
}
static void add_row (JustshotQuickSetting *self, gint row_index) {
  const RowDef *def = &rows[row_index]; GtkWidget *row, *box, *lbl, *check;
  if (def->type == ROW_SEPARATOR) { row = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL); gtk_list_box_insert (self->list_box, row, -1); gtk_widget_show (row); return; }
  row = gtk_list_box_row_new (); box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 12); gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 8); gtk_widget_set_margin_bottom (box, 8);
  lbl = gtk_label_new (def->label); gtk_widget_set_halign (lbl, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (box), lbl, TRUE, TRUE, 0);
  check = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (check), 16); gtk_box_pack_end (GTK_BOX (box), check, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (row), box); g_object_set_data (G_OBJECT (row), "row-type", GINT_TO_POINTER (def->type));
  self->check_widgets[row_index] = check; gtk_list_box_insert (self->list_box, row, -1); gtk_widget_show_all (row);
}
static void update_checkmarks (JustshotQuickSetting *self) {
  gint i; const char *text;
  for (i = 0; i < 7; i++) {
    if (!self->check_widgets[i]) continue; gboolean visible = FALSE;
    if (rows[i].type == ROW_MODE_FULL && !self->area_mode) visible = TRUE;
    else if (rows[i].type == ROW_MODE_AREA && self->area_mode) visible = TRUE;
    else if (rows[i].type == ROW_DELAY_0 && self->selected_delay == 0) visible = TRUE;
    else if (rows[i].type == ROW_DELAY_1 && self->selected_delay == 1) visible = TRUE;
    else if (rows[i].type == ROW_DELAY_3 && self->selected_delay == 3) visible = TRUE;
    else if (rows[i].type == ROW_DELAY_10 && self->selected_delay == 10) visible = TRUE;
    gtk_widget_set_visible (self->check_widgets[i], visible);
  }
  if (self->area_mode) { switch (self->selected_delay) { case 0: text=_("Area screenshot now"); break; case 1: text=_("Area screenshot in 1s"); break; case 3: text=_("Area screenshot in 3s"); break; case 10: text=_("Area screenshot in 10s"); break; default: text=_("Area screenshot"); break; } }
  else { switch (self->selected_delay) { case 0: text=_("Screenshot now"); break; case 1: text=_("Screenshot in 1s"); break; case 3: text=_("Screenshot in 3s"); break; case 10: text=_("Screenshot in 10s"); break; default: text=_("Screenshot"); break; } }
  gtk_label_set_text (self->btn_label, text);
}
static void on_row_activated (JustshotQuickSetting *self, GtkListBoxRow *row) {
  gint type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "row-type"));
  switch (type) { case ROW_MODE_FULL: self->area_mode = FALSE; break; case ROW_MODE_AREA: self->area_mode = TRUE; break;
    case ROW_DELAY_0: self->selected_delay = 0; g_settings_set_uint (self->settings, "capture-delay", 0); break;
    case ROW_DELAY_1: self->selected_delay = 1; g_settings_set_uint (self->settings, "capture-delay", 1); break;
    case ROW_DELAY_3: self->selected_delay = 3; g_settings_set_uint (self->settings, "capture-delay", 3); break;
    case ROW_DELAY_10: self->selected_delay = 10; g_settings_set_uint (self->settings, "capture-delay", 10); break;
    default: return; } update_checkmarks (self);
}
static void on_capture_clicked (JustshotQuickSetting *self) {
  CaptureData *data; g_message ("justshot: on_capture_clicked delay=%u area=%d", self->selected_delay, self->area_mode);
  data = g_new0 (CaptureData, 1); data->area_mode = self->area_mode;
  phosh_quick_setting_set_showing_status (PHOSH_QUICK_SETTING (self), FALSE);
  if (self->selected_delay > 0) { g_message ("justshot: scheduling in %us", self->selected_delay); g_timeout_add_seconds (self->selected_delay, do_capture, data); }
  else { g_message ("justshot: immediate"); do_capture (data); }
}
static void on_clicked (JustshotQuickSetting *self) { }
static void justshot_quick_setting_class_init (JustshotQuickSettingClass *klass) {
  GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass);
  gtk_widget_class_set_template_from_resource (wc, "/mobi/phosh/plugins/justshot-quick-setting/qs.ui");
  gtk_widget_class_bind_template_child (wc, JustshotQuickSetting, info);
  gtk_widget_class_bind_template_child (wc, JustshotQuickSetting, status_page);
  gtk_widget_class_bind_template_child (wc, JustshotQuickSetting, list_box);
  gtk_widget_class_bind_template_child (wc, JustshotQuickSetting, btn_label);
  gtk_widget_class_bind_template_callback (wc, on_clicked);
  gtk_widget_class_bind_template_callback (wc, on_row_activated);
  gtk_widget_class_bind_template_callback (wc, on_capture_clicked);
}
static void justshot_quick_setting_init (JustshotQuickSetting *self) {
  gtk_widget_init_template (GTK_WIDGET (self));
  self->settings = g_settings_new ("org.just.JustShot");
  self->selected_delay = g_settings_get_uint (self->settings, "capture-delay");
  if (self->selected_delay == 0) self->selected_delay = 1;
  self->area_mode = FALSE;
  for (gint i = 0; i < 7; i++) add_row (self, i);
  update_checkmarks (self);
  g_message ("justshot: plugin init delay=%u", self->selected_delay);
}