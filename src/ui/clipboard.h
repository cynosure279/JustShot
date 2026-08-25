/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_CLIPBOARD_H
#define JUST_SHOT_CLIPBOARD_H

#include <gtk/gtk.h>
#include <cairo.h>

void clipboard_copy_surface (cairo_surface_t *surface);
gboolean clipboard_has_image (void);

#endif /* JUST_SHOT_CLIPBOARD_H */
