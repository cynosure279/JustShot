/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "export.h"
#include <glib/gstdio.h>

typedef struct {
  cairo_surface_t *surface;
  gchar           *output_path;
} ExportData;

static void
export_data_free (ExportData *data)
{
  if (data == NULL) return;
  if (data->surface) cairo_surface_destroy (data->surface);
  g_free (data->output_path);
  g_free (data);
}

static void
export_thread_func (GTask        *task,
                    gpointer      source_object G_GNUC_UNUSED,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  ExportData *data = task_data;
  GError *error = NULL;

  /* Convert surface to pixbuf */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (data->surface, 0, 0,
                                                     cairo_image_surface_get_width (data->surface),
                                                     cairo_image_surface_get_height (data->surface));
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (pixbuf == NULL)
    {
      g_task_return_new_error (task, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                               "Failed to create pixbuf from surface");
      return;
    }

  /* Save as PNG (synchronous) */
  gboolean success = gdk_pixbuf_savev (pixbuf, data->output_path, "png",
                                        NULL, NULL, &error);
  g_object_unref (pixbuf);

  if (!success)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

void
export_to_png_async (cairo_surface_t    *surface,
                     const gchar        *output_path,
                     GCancellable       *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer            user_data)
{
  g_return_if_fail (surface != NULL);
  g_return_if_fail (output_path != NULL);

  ExportData *data = g_new0 (ExportData, 1);
  data->surface = cairo_surface_reference (surface);
  data->output_path = g_strdup (output_path);

  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, data, (GDestroyNotify) export_data_free);
  g_task_run_in_thread (task, export_thread_func);
  g_object_unref (task);
}

gboolean
export_to_png_finish (GAsyncResult *result,
                      GError      **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}
