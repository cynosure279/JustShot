/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_WINDOW_H
#define JUST_SHOT_WINDOW_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../application.h"
#include "../document.h"
#include "../capture-controller.h"

#define JUST_SHOT_TYPE_WINDOW (just_shot_window_get_type ())
G_DECLARE_FINAL_TYPE (JustShotWindow, just_shot_window, JUST_SHOT, WINDOW, AdwApplicationWindow)

JustShotWindow *just_shot_window_new (JustShotApplication *app);
void just_shot_window_show_preview (JustShotWindow *win, ImageDocument *doc);
void just_shot_window_set_state (JustShotWindow *win, CaptureState state);

#endif /* JUST_SHOT_WINDOW_H */
