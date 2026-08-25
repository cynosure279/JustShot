/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_PREVIEW_H
#define JUST_SHOT_PREVIEW_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../document.h"

#define JUST_SHOT_TYPE_PREVIEW (just_shot_preview_get_type ())
G_DECLARE_FINAL_TYPE (JustShotPreview, just_shot_preview, JUST_SHOT, PREVIEW, GtkDrawingArea)

GtkWidget *just_shot_preview_new (void);
void just_shot_preview_set_document (JustShotPreview *preview, ImageDocument *doc);
void just_shot_preview_set_zoom (JustShotPreview *preview, gdouble zoom);
gdouble just_shot_preview_get_zoom (JustShotPreview *preview);
void just_shot_preview_zoom_in (JustShotPreview *preview);
void just_shot_preview_zoom_out (JustShotPreview *preview);
void just_shot_preview_zoom_fit (JustShotPreview *preview);

#endif /* JUST_SHOT_PREVIEW_H */
