/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_EXPORT_H
#define JUST_SHOT_EXPORT_H

#include <gtk/gtk.h>
#include <cairo.h>

void export_to_png_async (
    cairo_surface_t   *surface,
    const gchar       *output_path,
    GCancellable      *cancellable,
    GAsyncReadyCallback callback,
    gpointer           user_data);

gboolean export_to_png_finish (
    GAsyncResult *result,
    GError      **error);

#endif /* JUST_SHOT_EXPORT_H */
