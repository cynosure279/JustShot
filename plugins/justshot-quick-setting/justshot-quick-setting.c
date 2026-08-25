/*
 * JustShot Quick Setting — PhoshStatusPage fold pattern.
 * List has two sections: capture mode (full/area) and delay.
 */
#include "justshot-quick-setting.h"
#include <gio/gio.h>

#define JUST_SHOT_BIN "/usr/local/bin/justshot"

/* Row types */
enum {
  ROW_MODE_FULL,
  ROW_MODE_AREA,
  ROW_DELAY_0,
  ROW_DELAY_1,
  ROW_DELAY_3,
  ROW_DELAY_10,
  ROW_SEPARATOR,
};

typedef struct {
  gint type;
  const gchar *label;
} RowDef;

static const RowDef rows[] = {
  { ROW_MODE_FULL,  "🖥️ 全屏截图" },
  { ROW_MODE_AREA,  "✂️ 区域框选" },
  { ROW_SEPARATOR,  NULL },
  { ROW_DELAY_0,    "⏱️ 立即 (0s)" },
  { ROW_DELAY_1,    "⏱️ 1 秒" },
  { ROW_DELAY_3,    "⏱️ 3 秒" },
  { ROW_DELAY_10,   "⏱️ 10 秒" },
};

struct _JustshotQuickSetting {
  PhoshQuickSetting parent;
  PhoshStatusIcon   *info;
  PhoshStatusPage   *status_page;
  GtkListBox        *list_box;
  GtkLabel          *btn_label;
  GSettings         *settings;
  gboolean           area_mode;
  guint              selected_delay;
  GtkWidget         *check_widgets[7]; /* checkmark for each row */
};

G_DEFINE_TYPE (JustshotQuickSetting, justshot_quick_setting, PHOSH_TYPE_QUICK_SETTING)

static void
add_row (JustshotQuickSetting *self, gint row_index)
{
  const RowDef *def = &rows[row_index];

  if (def->type == ROW_SEPARATOR)
    {
      GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_insert (self->list_box, sep, -1);
      gtk_widget_show (sep);
      return;
    }

  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 8);
  gtk_widget_set_margin_bottom (box, 8);

  GtkWidget *lbl = gtk_label_new (def->label);
  gtk_widget_set_halign (lbl, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (box), lbl, TRUE, TRUE, 0);

  GtkWidget *check = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (check), 16);
  gtk_box_pack_end (GTK_BOX (box), check, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data (G_OBJECT (row), "row-type", GINT_TO_POINTER (def->type));
  self->check_widgets[row_index] = check;

  gtk_list_box_insert (self->list_box, row, -1);
  gtk_widget_show_all (row);
}

static void
update_checkmarks (JustshotQuickSetting *self)
{
  for (gint i = 0; i < 7; i++)
    {
      if (!self->check_widgets[i])
        continue;
      gboolean visible = FALSE;
      if (rows[i].type == ROW_MODE_FULL && !self->area_mode)
        visible = TRUE;
      else if (rows[i].type == ROW_MODE_AREA && self->area_mode)
        visible = TRUE;
      else if (rows[i].type == ROW_DELAY_0 && self->selected_delay == 0)
        visible = TRUE;
      else if (rows[i].type == ROW_DELAY_1 && self->selected_delay == 1)
        visible = TRUE;
      else if (rows[i].type == ROW_DELAY_3 && self->selected_delay == 3)
        visible = TRUE;
      else if (rows[i].type == ROW_DELAY_10 && self->selected_delay == 10)
        visible = TRUE;
      gtk_widget_set_visible (self->check_widgets[i], visible);
    }

  /* Update footer button label */
  const gchar *text;
  if (self->area_mode)
    {
      switch (self->selected_delay)
        {
        case 0:  text = "✂️ 立即框选"; break;
        case 1:  text = "✂️ 1秒后框选"; break;
        case 3:  text = "✂️ 3秒后框选"; break;
        case 10: text = "✂️ 10秒后框选"; break;
        default: text = "✂️ 框选截图"; break;
        }
    }
  else
    {
      switch (self->selected_delay)
        {
        case 0:  text = "📸 立即截图"; break;
        case 1:  text = "📸 1秒后截图"; break;
        case 3:  text = "📸 3秒后截图"; break;
        case 10: text = "📸 10秒后截图"; break;
        default: text = "📸 截图"; break;
        }
    }
  gtk_label_set_text (self->btn_label, text);
}

static void
on_row_activated (JustshotQuickSetting *self, GtkListBoxRow *row)
{
  gint type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "row-type"));

  switch (type)
    {
    case ROW_MODE_FULL:
      self->area_mode = FALSE;
      break;
    case ROW_MODE_AREA:
      self->area_mode = TRUE;
      break;
    case ROW_DELAY_0:
      self->selected_delay = 0;
      g_settings_set_uint (self->settings, "capture-delay", 0);
      break;
    case ROW_DELAY_1:
      self->selected_delay = 1;
      g_settings_set_uint (self->settings, "capture-delay", 1);
      break;
    case ROW_DELAY_3:
      self->selected_delay = 3;
      g_settings_set_uint (self->settings, "capture-delay", 3);
      break;
    case ROW_DELAY_10:
      self->selected_delay = 10;
      g_settings_set_uint (self->settings, "capture-delay", 10);
      break;
    default:
      return;
    }

  update_checkmarks (self);
}

static void
on_capture_clicked (JustshotQuickSetting *self)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *delay_str = g_strdup_printf ("%u", self->selected_delay);

  /* Build argv: justshot --delay N [--area] */
  GPtrArray *argv = g_ptr_array_new ();
  g_ptr_array_add (argv, g_strdup (JUST_SHOT_BIN));
  if (self->selected_delay > 0)
    {
      g_ptr_array_add (argv, g_strdup ("--delay"));
      g_ptr_array_add (argv, g_steal_pointer (&delay_str));
    }
  if (self->area_mode)
    g_ptr_array_add (argv, g_strdup ("--area"));
  g_ptr_array_add (argv, NULL);

  g_subprocess_newv ((const gchar * const *) argv->pdata, G_SUBPROCESS_FLAGS_NONE, &error);
  if (error)
    g_warning ("Failed to launch justshot: %s", error->message);

  g_ptr_array_free (argv, TRUE);

  /* Fold the panel back */
  phosh_quick_setting_set_showing_status (PHOSH_QUICK_SETTING (self), FALSE);
}

static void
on_clicked (JustshotQuickSetting *self)
{
  /* Shell handles the fold toggle */
}

static void
justshot_quick_setting_class_init (JustshotQuickSettingClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
    "/org/just/justshot/plugins/justshot-quick-setting/qs.ui");
  gtk_widget_class_bind_template_child (widget_class, JustshotQuickSetting, info);
  gtk_widget_class_bind_template_child (widget_class, JustshotQuickSetting, status_page);
  gtk_widget_class_bind_template_child (widget_class, JustshotQuickSetting, list_box);
  gtk_widget_class_bind_template_child (widget_class, JustshotQuickSetting, btn_label);
  gtk_widget_class_bind_template_callback (widget_class, on_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_capture_clicked);
}

static void
justshot_quick_setting_init (JustshotQuickSetting *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("org.just.JustShot");
  self->selected_delay = g_settings_get_uint (self->settings, "capture-delay");
  if (self->selected_delay == 0)
    self->selected_delay = 1; /* default 1s */
  self->area_mode = FALSE;

  /* Build rows */
  for (gint i = 0; i < 7; i++)
    add_row (self, i);

  update_checkmarks (self);
}
