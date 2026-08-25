/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_RENDERER_H
#define JUST_SHOT_RENDERER_H

#include <cairo.h>
#include <glib.h>
#include "document.h"

cairo_surface_t *renderer_render (ImageDocument *doc, GError **error);
cairo_surface_t *renderer_render_from (ImageDocument *doc, GList *start_op, GError **error);

#endif /* JUST_SHOT_RENDERER_H */
