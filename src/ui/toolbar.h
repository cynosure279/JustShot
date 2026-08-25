/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_TOOLBAR_H
#define JUST_SHOT_TOOLBAR_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../document.h"

#define JUST_SHOT_TYPE_TOOLBAR (just_shot_toolbar_get_type ())
G_DECLARE_FINAL_TYPE (JustShotToolbar, just_shot_toolbar, JUST_SHOT, TOOLBAR, GtkBox)

GtkWidget *just_shot_toolbar_new (void);
void just_shot_toolbar_set_document (JustShotToolbar *toolbar, ImageDocument *doc);
void just_shot_toolbar_set_crop_mode (JustShotToolbar *toolbar, gboolean active);
void just_shot_toolbar_set_draw_mode (JustShotToolbar *toolbar, DocumentOpType draw_type);

#endif /* JUST_SHOT_TOOLBAR_H */
