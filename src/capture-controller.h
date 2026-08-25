/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_CAPTURE_CONTROLLER_H
#define JUST_SHOT_CAPTURE_CONTROLLER_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include <justcapture/justcapture.h>
#include "document.h"

#define JUST_SHOT_TYPE_CAPTURE_CONTROLLER (capture_controller_get_type ())
G_DECLARE_FINAL_TYPE (CaptureController, capture_controller, CAPTURE, CONTROLLER, GObject)

typedef enum {
  CAPTURE_STATE_IDLE,
  CAPTURE_STATE_PROBING,
  CAPTURE_STATE_WAITING_DELAY,
  CAPTURE_STATE_REQUESTING,
  CAPTURE_STATE_DECODING,
  CAPTURE_STATE_EDITING,
  CAPTURE_STATE_SAVING,
  CAPTURE_STATE_CANCELLED,
  CAPTURE_STATE_ERROR,
} CaptureState;

CaptureController *capture_controller_new (GApplication *app);

void capture_controller_capture_async (
    CaptureController            *self,
    JustCaptureScreenshotTarget   target,
    guint                         delay_seconds,
    gboolean                      interactive,
    GCancellable                 *cancellable,
    GAsyncReadyCallback           callback,
    gpointer                      user_data);

gboolean capture_controller_capture_finish (
    CaptureController *self,
    GAsyncResult      *result,
    GError           **error);

CaptureState capture_controller_get_state (CaptureController *self);
JustCaptureScreenshotTarget capture_controller_get_available_targets (CaptureController *self);
ImageDocument *capture_controller_get_document (CaptureController *self);

#endif /* JUST_SHOT_CAPTURE_CONTROLLER_H */
