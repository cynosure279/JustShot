/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_APPLICATION_H
#define JUST_SHOT_APPLICATION_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include "capture-controller.h"

#define JUST_SHOT_APPLICATION_ID "org.just.JustShot"

#define JUST_SHOT_TYPE_APPLICATION (just_shot_application_get_type ())
G_DECLARE_FINAL_TYPE (JustShotApplication, just_shot_application, JUST_SHOT, APPLICATION, GtkApplication)

JustShotApplication *just_shot_application_new (void);
CaptureController *just_shot_application_get_controller (JustShotApplication *app);

#endif /* JUST_SHOT_APPLICATION_H */
