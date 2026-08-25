/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "window.h"

/* Forward declaration */
static void on_save_dialog_response (GObject *source, GAsyncResult *result, gpointer user_data);
#include "preview.h"
#include "toolbar.h"
#include "clipboard.h"
#include "../export.h"

struct _JustShotWindow {
  AdwApplicationWindow  parent_instance;
  JustShotApplication  *app;
  JustShotPreview      *preview;
  GtkWidget            *toolbar;
  ImageDocument        *document;
  gchar                *current_save_path;
};

G_DEFINE_TYPE (JustShotWindow, just_shot_window, ADW_TYPE_APPLICATION_WINDOW)

static void
on_save_clicked (GtkButton *button, gpointer user_data)
{
  JustShotWindow *self = JUST_SHOT_WINDOW (user_data);
  GtkWidget *dialog;
  GtkFileDialog *file_dialog;
  GFile *initial = NULL;

  if (self->current_save_path)
    initial = g_file_new_for_path (self->current_save_path);

  file_dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (file_dialog, "Save Screenshot");
  if (initial)
    gtk_file_dialog_set_initial_file (file_dialog, initial);

  gtk_file_dialog_save (file_dialog, GTK_WINDOW (self), NULL, on_save_dialog_response, self);

  g_object_unref (file_dialog);
  if (initial)
    g_object_unref (initial);
}

static void
on_copy_clicked (GtkButton *button, gpointer user_data)
{
  JustShotWindow *self = JUST_SHOT_WINDOW (user_data);
  cairo_surface_t *surface;

  if (!self->document)
    return;

  surface = image_document_render (self->document, NULL);
  if (surface) {
    clipboard_copy_surface (surface);
    cairo_surface_destroy (surface);
  }
}

static void
on_save_dialog_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GtkFileDialog *dlg = GTK_FILE_DIALOG (source);
  JustShotWindow *self = JUST_SHOT_WINDOW (user_data);
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_save_finish (dlg, result, &error);
  if (!file) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Save failed: %s", error->message);
    g_error_free (error);
    return;
  }

  g_free (self->current_save_path);
  self->current_save_path = g_file_get_path (file);

  if (self->document) {
    image_document_export_png_async (self->document,
                                      self->current_save_path,
                                      NULL, NULL, NULL);
    image_document_mark_saved (self->document);
  }

  g_object_unref (file);
}

static void
just_shot_window_class_init (JustShotWindowClass *klass)
{
}

static void
just_shot_window_init (JustShotWindow *self)
{
  AdwToolbarView *toolbar_view;
  AdwHeaderBar *header_bar;
  GtkWidget *box, *preview_box, *action_bar, *hbox;
  GtkWidget *save_btn, *copy_btn;
  GtkWidget *overlay;

  /* Main layout */
  toolbar_view = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
  gtk_widget_set_vexpand (GTK_WIDGET (toolbar_view), TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (toolbar_view), TRUE);

  /* Header bar */
  header_bar = ADW_HEADER_BAR (adw_header_bar_new ());
  adw_toolbar_view_add_top_bar (toolbar_view, GTK_WIDGET (header_bar));

  /* Content box */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_vexpand (box, TRUE);
  gtk_widget_set_hexpand (box, TRUE);

  /* Preview */
  overlay = gtk_overlay_new ();
  gtk_widget_set_vexpand (overlay, TRUE);
  gtk_widget_set_hexpand (overlay, TRUE);

  self->preview = JUST_SHOT_PREVIEW (just_shot_preview_new ());
  gtk_widget_set_vexpand (GTK_WIDGET (self->preview), TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->preview), TRUE);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), GTK_WIDGET (self->preview));

  gtk_box_append (GTK_BOX (box), overlay);

  /* Toolbar (editing tools) */
  self->toolbar = just_shot_toolbar_new ();
  gtk_box_append (GTK_BOX (box), self->toolbar);

  /* Action bar (save/copy/share) */
  action_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (action_bar, 12);
  gtk_widget_set_margin_end (action_bar, 12);
  gtk_widget_set_margin_top (action_bar, 6);
  gtk_widget_set_margin_bottom (action_bar, 6);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (hbox, GTK_ALIGN_END);

  save_btn = gtk_button_new_with_label ("Save");
  g_signal_connect (save_btn, "clicked", G_CALLBACK (on_save_clicked), self);
  gtk_box_append (GTK_BOX (hbox), save_btn);

  copy_btn = gtk_button_new_with_label ("Copy");
  g_signal_connect (copy_btn, "clicked", G_CALLBACK (on_copy_clicked), self);
  gtk_box_append (GTK_BOX (hbox), copy_btn);

  gtk_box_append (GTK_BOX (action_bar), hbox);
  gtk_box_append (GTK_BOX (box), action_bar);

  adw_toolbar_view_set_content (toolbar_view, box);
  gtk_window_set_child (GTK_WINDOW (self), GTK_WIDGET (toolbar_view));

  /* Window defaults */
  gtk_window_set_default_size (GTK_WINDOW (self), 800, 600);
  gtk_window_set_title (GTK_WINDOW (self), "JustShot");
}

JustShotWindow *
just_shot_window_new (JustShotApplication *app)
{
  JustShotWindow *win;

  win = g_object_new (JUST_SHOT_TYPE_WINDOW,
                      "application", app,
                      NULL);
  win->app = app;
  win->document = NULL;
  win->current_save_path = NULL;

  return win;
}

void
just_shot_window_show_preview (JustShotWindow *win, ImageDocument *doc)
{
  g_return_if_fail (JUST_SHOT_IS_WINDOW (win));

  win->document = doc;
  just_shot_preview_set_document (win->preview, doc);
  just_shot_toolbar_set_document (JUST_SHOT_TOOLBAR (win->toolbar), doc);

  gtk_window_present (GTK_WINDOW (win));
}

void
just_shot_window_set_state (JustShotWindow *win, CaptureState state)
{
  g_return_if_fail (JUST_SHOT_IS_WINDOW (win));

  /* Update UI based on capture state */
  switch (state) {
    case CAPTURE_STATE_IDLE:
      gtk_window_set_title (GTK_WINDOW (win), "JustShot");
      break;
    case CAPTURE_STATE_REQUESTING:
      gtk_window_set_title (GTK_WINDOW (win), "JustShot — Capturing...");
      break;
    case CAPTURE_STATE_EDITING:
      gtk_window_set_title (GTK_WINDOW (win), "JustShot — Edit");
      break;
    case CAPTURE_STATE_SAVING:
      gtk_window_set_title (GTK_WINDOW (win), "JustShot — Saving...");
      break;
    case CAPTURE_STATE_ERROR:
      gtk_window_set_title (GTK_WINDOW (win), "JustShot — Error");
      break;
    default:
      break;
  }
}