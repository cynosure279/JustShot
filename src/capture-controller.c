/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "capture-controller.h"
#include <unistd.h>
#include "export.h"
#include "ui/window.h"
#include "ui/clipboard.h"
#include "cli/cli.h"

struct _CaptureController {
  GObject                   parent_instance;
  GApplication             *app;
  JustCapturePortal        *portal;
  JustCaptureCapabilities  *capabilities;
  CaptureState              state;
  ImageDocument            *document;
  gchar                    *output_path;
  gboolean                  copy_to_clipboard;
};

G_DEFINE_TYPE (CaptureController, capture_controller, G_TYPE_OBJECT)

enum {
  SIGNAL_STATE_CHANGED,
  SIGNAL_CAPTURE_COMPLETE,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
capture_controller_set_state (CaptureController *self, CaptureState state)
{
  self->state = state;
  g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, state);
}

/* Callback when screenshot is complete */
static void
on_screenshot_complete (GObject      *source G_GNUC_UNUSED,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  CaptureController *self = CAPTURE_CONTROLLER (g_task_get_source_object (task));
  GError *error = NULL;
  JustCaptureScreenshotResult *screenshot_result;
  gchar *uri = NULL;
  gchar *tmp_path = NULL;
  ImageDocument *doc = NULL;

  screenshot_result = just_capture_screenshot_request_finish (self->portal, result, &error);
  if (!screenshot_result) {
    if (g_error_matches (error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_CANCELLED)) {
      capture_controller_set_state (self, CAPTURE_STATE_CANCELLED);
      g_task_return_boolean (task, FALSE);
    } else {
      capture_controller_set_state (self, CAPTURE_STATE_ERROR);
      g_task_return_error (task, error);
    }
    g_object_unref (task);
    return;
  }

  capture_controller_set_state (self, CAPTURE_STATE_DECODING);
  uri = screenshot_result->uri;

  /* Handle document: protocol - read from Documents Portal */
  if (g_str_has_prefix (uri, "document:")) {
    GFile *file = g_file_new_for_uri (uri);
    gchar *path = g_file_get_path (file);
    if (path) {
      tmp_path = g_strdup (path);
      g_free (path);
    } else {
      /* Try to read the file contents via GIO */
      gchar *contents = NULL;
      gsize len = 0;
      if (g_file_load_contents (file, NULL, &contents, &len, NULL, &error)) {
        gint fd;
        gchar *tmpl = g_strdup_printf ("/tmp/justshot-XXXXXX");
        fd = g_mkstemp (tmpl);
        if (fd != -1) {
          gsize written = 0;
          while (written < len) {
            gssize w = write (fd, contents + written, len - written);
            if (w < 0) break;
            written += w;
          }
          close (fd);
          tmp_path = tmpl;
        } else {
          g_free (tmpl);
        }
        g_free (contents);
      }
    }
    g_object_unref (file);
  } else if (g_str_has_prefix (uri, "file:")) {
    GFile *file = g_file_new_for_uri (uri);
    tmp_path = g_file_get_path (file);
    g_object_unref (file);
  }

  if (!tmp_path && !error) {
    g_set_error (&error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED,
                 "Could not resolve screenshot URI: %s", uri);
  }

  if (error) {
    capture_controller_set_state (self, CAPTURE_STATE_ERROR);
    g_task_return_error (task, error);
    g_object_unref (task);
    just_capture_screenshot_result_free (screenshot_result);
    return;
  }

  /* Load document */
  doc = image_document_new_from_file (tmp_path, &error);
  g_free (tmp_path);

  if (!doc) {
    capture_controller_set_state (self, CAPTURE_STATE_ERROR);
    g_task_return_error (task, error);
    g_object_unref (task);
    just_capture_screenshot_result_free (screenshot_result);
    return;
  }

  self->document = doc;

  just_capture_screenshot_result_free (screenshot_result);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

/* Capture delay callback */
static gboolean
on_delay_elapsed (gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  CaptureController *self;
  gpointer src_obj;

  src_obj = g_task_get_source_object (task);
  self = CAPTURE_CONTROLLER (src_obj);

  capture_controller_set_state (self, CAPTURE_STATE_REQUESTING);

  just_capture_screenshot_request_async (
      self->portal,
      (JustCaptureScreenshotTarget) GPOINTER_TO_UINT (g_task_get_task_data (task)),
      TRUE,
      "",
      g_task_get_cancellable (task),
      on_screenshot_complete,
      task);

  return G_SOURCE_REMOVE;
}

/* Capabilities query callback */
static void
on_capabilities_queried (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  CaptureController *self = CAPTURE_CONTROLLER (user_data);
  GError *error = NULL;

  self->capabilities = just_capture_capabilities_query_finish (self->portal, result, &error);
  if (error) {
    g_warning ("Failed to query capabilities: %s", error->message);
    g_error_free (error);
  }
}

/* Main async capture entry point */
void
capture_controller_capture_async (CaptureController            *self,
                                  JustCaptureScreenshotTarget   target,
                                  guint                         delay_seconds,
                                  gboolean                      interactive,
                                  GCancellable                 *cancellable,
                                  GAsyncReadyCallback           callback,
                                  gpointer                      user_data)
{
  GTask *task;

  g_return_if_fail (CAPTURE_IS_CONTROLLER (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, GUINT_TO_POINTER (target), NULL);

  /* Probe capabilities if not yet done */
  if (!self->capabilities) {
    capture_controller_set_state (self, CAPTURE_STATE_PROBING);
    just_capture_capabilities_query_async (self->portal, cancellable,
                                           on_capabilities_queried, self);
  }

  /* Handle delay */
  if (delay_seconds > 0) {
    capture_controller_set_state (self, CAPTURE_STATE_WAITING_DELAY);
    g_timeout_add_seconds (delay_seconds, on_delay_elapsed, task);
    return;
  }

  capture_controller_set_state (self, CAPTURE_STATE_REQUESTING);

  just_capture_screenshot_request_async (
      self->portal,
      target,
      interactive || target == JUST_CAPTURE_SCREENSHOT_TARGET_AREA,
      "",
      cancellable,
      on_screenshot_complete,
      task);
}

gboolean
capture_controller_capture_finish (CaptureController *self,
                                   GAsyncResult      *result,
                                   GError           **error)
{
  g_return_val_if_fail (CAPTURE_IS_CONTROLLER (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

CaptureState
capture_controller_get_state (CaptureController *self)
{
  g_return_val_if_fail (CAPTURE_IS_CONTROLLER (self), CAPTURE_STATE_IDLE);
  return self->state;
}

JustCaptureScreenshotTarget
capture_controller_get_available_targets (CaptureController *self)
{
  g_return_val_if_fail (CAPTURE_IS_CONTROLLER (self), 0);
  if (self->capabilities)
    return self->capabilities->screenshot_targets;
  return JUST_CAPTURE_SCREENSHOT_TARGET_SCREEN |
         JUST_CAPTURE_SCREENSHOT_TARGET_WINDOW |
         JUST_CAPTURE_SCREENSHOT_TARGET_AREA |
         JUST_CAPTURE_SCREENSHOT_TARGET_ACTIVE_WINDOW;
}

ImageDocument *
capture_controller_get_document (CaptureController *self)
{
  g_return_val_if_fail (CAPTURE_IS_CONTROLLER (self), NULL);
  return self->document;
}

static void
capture_controller_finalize (GObject *object)
{
  CaptureController *self = CAPTURE_CONTROLLER (object);

  g_clear_pointer (&self->capabilities, just_capture_capabilities_free);
  g_clear_pointer (&self->document, image_document_free);
  g_free (self->output_path);

  G_OBJECT_CLASS (capture_controller_parent_class)->finalize (object);
}

static void
capture_controller_init (CaptureController *self)
{
  self->state = CAPTURE_STATE_IDLE;
  self->portal = just_capture_portal_get_default ();
  self->capabilities = NULL;
  self->document = NULL;
  self->output_path = NULL;
  self->copy_to_clipboard = FALSE;
}

static void
capture_controller_class_init (CaptureControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = capture_controller_finalize;

  signals[SIGNAL_STATE_CHANGED] = g_signal_new ("state-changed",
      JUST_SHOT_TYPE_CAPTURE_CONTROLLER, G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
      NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  signals[SIGNAL_CAPTURE_COMPLETE] = g_signal_new ("capture-complete",
      JUST_SHOT_TYPE_CAPTURE_CONTROLLER, G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
      NULL, G_TYPE_NONE, 0);
}

CaptureController *
capture_controller_new (GApplication *app)
{
  return g_object_new (JUST_SHOT_TYPE_CAPTURE_CONTROLLER,
                       NULL);
}
