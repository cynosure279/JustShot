/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "cli.h"
#include "../application.h"
#include "../capture-controller.h"
#include "../export.h"
#include "../ui/window.h"
#include "../ui/clipboard.h"
#include <justcapture/justcapture.h>

gboolean
cli_parse_args (GApplication *app, int argc, char *argv[], CliOptions *opts)
{
  GOptionEntry entries[] = {
    { "target", 't', 0, G_OPTION_ARG_INT, &opts->target,
      "Capture target (0=default, 1=screen, 2=window, 4=area, 8=active-window)", "TARGET" },
    { "delay", 'd', 0, G_OPTION_ARG_INT, &opts->delay_seconds,
      "Delay in seconds before capture", "SECONDS" },
    { "output", 'o', 0, G_OPTION_ARG_STRING, &opts->output_path,
      "Output file path", "PATH" },
    { "clipboard", 'c', 0, G_OPTION_ARG_NONE, &opts->clipboard,
      "Copy screenshot to clipboard", NULL },
    { "interactive", 'i', 0, G_OPTION_ARG_NONE, &opts->interactive,
      "Open editor after capture", NULL },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &opts->quiet,
      "Quiet mode (no GUI)", NULL },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opts->version,
      "Show version", NULL },
    { NULL }
  };
  GOptionContext *context;
  GError *error = NULL;
  gboolean ret;

  memset (opts, 0, sizeof (CliOptions));

  context = g_option_context_new ("- Screenshot tool");
  g_option_context_set_ignore_unknown_options (context, FALSE);
  g_option_context_set_help_enabled (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  ret = g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

  if (!ret) {
    g_printerr ("Error parsing options: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

void
cli_options_free (CliOptions *opts)
{
  g_free (opts->output_path);
  memset (opts, 0, sizeof (CliOptions));
}

typedef struct {
  CliOptions opts;
  gchar     *output_path;
} CliCaptureData;

static void
cli_capture_data_free (CliCaptureData *data)
{
  g_free (data->output_path);
  cli_options_free (&data->opts);
  g_free (data);
}

static void
on_cli_export_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (!export_to_png_finish (result, &error)) {
    g_printerr ("Export failed: %s\n", error->message);
    g_error_free (error);
    g_task_return_boolean (task, FALSE);
  } else {
    g_task_return_boolean (task, TRUE);
  }
  g_object_unref (task);
}

static void
on_cli_capture_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  CliCaptureData *data = (CliCaptureData *) g_task_get_task_data (task);
  CaptureController *controller = CAPTURE_CONTROLLER (source);
  GError *error = NULL;
  ImageDocument *doc;
  cairo_surface_t *surface;

  if (!capture_controller_capture_finish (controller, result, &error)) {
    if (error) {
      g_printerr ("Capture failed: %s\n", error->message);
      g_error_free (error);
    }
    g_task_return_boolean (task, FALSE);
    g_object_unref (task);
    return;
  }

  doc = capture_controller_get_document (controller);
  if (!doc) {
    g_printerr ("No screenshot captured\n");
    g_task_return_boolean (task, FALSE);
    g_object_unref (task);
    return;
  }

  /* Determine output path */
  if (!data->output_path) {
    gchar *dir = just_capture_output_path_get_screenshots_dir ();
    gchar *basename = just_capture_filename_make_screenshot (g_date_time_new_now_local ());
    data->output_path = just_capture_filename_make_unique (dir, basename, ".png", NULL);
    g_free (dir);
    g_free (basename);
  }

  if (!data->opts.quiet)
    g_print ("Saving screenshot to: %s\n", data->output_path);

  /* Export */
  image_document_export_png_async (doc, data->output_path, NULL, on_cli_export_done, task);

  /* Copy to clipboard if requested */
  if (data->opts.clipboard) {
    surface = image_document_render (doc, NULL);
    if (surface) {
      clipboard_copy_surface (surface);
      cairo_surface_destroy (surface);
    }
  }
}

void
cli_execute (GApplication *app, CliOptions *opts)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (app);
  CaptureController *controller;
  JustCaptureScreenshotTarget target;
  CliCaptureData *data;
  GTask *task;

  controller = just_shot_application_get_controller (self);

  /* Determine target */
  if (opts->target > 0)
    target = (JustCaptureScreenshotTarget) opts->target;
  else
    target = JUST_CAPTURE_SCREENSHOT_TARGET_SCREEN;

  /* If interactive, start capture and show window */
  if (opts->interactive) {
    capture_controller_capture_async (controller, target,
                                       opts->delay_seconds, TRUE,
                                       NULL, NULL, NULL);
    g_application_activate (app);
    return;
  }

  /* Non-interactive: capture then save */
  data = g_new0 (CliCaptureData, 1);
  data->opts = *opts;
  data->output_path = opts->output_path ? g_strdup (opts->output_path) : NULL;

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, data, (GDestroyNotify) cli_capture_data_free);

  capture_controller_capture_async (controller, target,
                                     opts->delay_seconds, FALSE,
                                     NULL,
                                     on_cli_capture_done, task);
}