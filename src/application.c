/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "application.h"
#include "cli/cli.h"
#include "ui/window.h"
#include <glib/gstdio.h>
#include <justcapture/justcapture.h>
#include <justcapture/filename.h>
#include <justcapture/output-path.h>

/* Phosh provides its own screenshot D-Bus interface that uses the built-in
 * wlr-screencopy. This works on devices where the xdg-desktop-portal path
 * fails (e.g. phoc without portal chooser support). */
#define PHOSH_SCREENSHOT_BUS   "org.gnome.Shell.Screenshot"
#define PHOSH_SCREENSHOT_PATH  "/org/gnome/Shell/Screenshot"
#define PHOSH_SCREENSHOT_IFACE "org.gnome.Shell.Screenshot"

static void
on_phosh_screenshot_done (GObject      *source G_GNUC_UNUSED,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GApplication *app = G_APPLICATION (user_data);
  GError *error = NULL;
  g_autoptr(GVariant) result = g_dbus_connection_call_finish (
      G_DBUS_CONNECTION (source), res, &error);

  if (result == NULL)
    {
      g_warning ("Phosh screenshot failed: %s", error->message);
      g_error_free (error);
      g_application_release (app);
      return;
    }

  gboolean success = FALSE;
  const gchar *filename_used = NULL;
  g_variant_get (result, "(b&s)", &success, &filename_used);

  if (success && filename_used)
    {
      g_autofree gchar *basename = just_capture_filename_make_screenshot (NULL);
      g_autofree gchar *dir = just_capture_output_path_get_screenshots_dir ();
      g_autoptr(GError) dir_err = NULL;
      if (just_capture_output_path_ensure_dir (dir, &dir_err))
        {
          g_autofree gchar *unique = just_capture_filename_make_unique (dir, basename, ".png", NULL);
          if (unique)
            {
              g_autoptr(GFile) src = g_file_new_for_path (filename_used);
              g_autoptr(GFile) dst = g_file_new_for_path (unique);
              g_autoptr(GError) copy_err = NULL;
              if (g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &copy_err))
                g_message ("Screenshot saved: %s", unique);
              else
                g_warning ("Copy failed: %s", copy_err->message);
            }
        }
      /* Clean up phosh's temp file */
      g_unlink (filename_used);
    }

  g_application_release (app);
}

static void
take_screenshot_phosh (GApplication *app)
{
  g_application_hold (app);

  GDBusConnection *conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (!conn)
    {
      g_warning ("Cannot connect to session bus");
      g_application_release (app);
      return;
    }

  /* Use a temp path; phosh writes the PNG here */
  g_autofree gchar *tmp_path = g_strdup_printf ("/tmp/justshot-%u.png", (guint) getpid ());

  g_dbus_connection_call (conn,
      PHOSH_SCREENSHOT_BUS,
      PHOSH_SCREENSHOT_PATH,
      PHOSH_SCREENSHOT_IFACE,
      "Screenshot",
      g_variant_new ("(bbs)", TRUE, FALSE, tmp_path),
      G_VARIANT_TYPE ("(bs)"),
      G_DBUS_CALL_FLAGS_NONE, 15000,
      NULL,
      on_phosh_screenshot_done,
      app);

  g_object_unref (conn);
}

struct _JustShotApplication {
  GtkApplication      parent_instance;
  CaptureController  *controller;
  JustCapturePortal  *portal;
  gboolean            cli_options_parsed;
};

G_DEFINE_TYPE (JustShotApplication, just_shot_application, GTK_TYPE_APPLICATION)

/* Simple screenshot callback — no GTask, just save the file */
/* Copy the portal-returned URI to ~/Pictures/Screenshots/ with a
 * proper timestamped filename, then release the app hold. */
static void
on_screenshot_done (GObject *source G_GNUC_UNUSED, GAsyncResult *res, gpointer user_data)
{
  GApplication *app = G_APPLICATION (user_data);
  GError *error = NULL;
  JustCaptureScreenshotResult *result;

  result = just_capture_screenshot_request_finish (NULL, res, &error);
  if (result == NULL)
    {
      if (error)
        {
          g_warning ("Screenshot failed: %s", error->message);
          g_error_free (error);
        }
      g_application_release (app);
      return;
    }

  if (result->uri)
    {
      g_autofree gchar *basename = just_capture_filename_make_screenshot (NULL);
      g_autofree gchar *dir = just_capture_output_path_get_screenshots_dir ();
      g_autoptr(GError) dir_err = NULL;
      if (!just_capture_output_path_ensure_dir (dir, &dir_err))
        {
          g_warning ("Cannot create screenshots dir: %s", dir_err->message);
        }
      else
        {
          g_autofree gchar *unique = just_capture_filename_make_unique (dir, basename, ".png", NULL);
          if (unique != NULL)
            {
              g_autoptr(GFile) src = g_file_new_for_uri (result->uri);
              g_autoptr(GFile) dst = g_file_new_for_path (unique);
              g_autoptr(GError) copy_err = NULL;
              if (g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &copy_err))
                g_message ("Screenshot saved: %s", unique);
              else
                g_warning ("Failed to copy screenshot to %s: %s", unique, copy_err->message);
            }
        }
    }

  just_capture_screenshot_result_free (result);
  g_application_release (app);
}

static void
just_shot_application_activate (GApplication *app)
{
  /* Take a screenshot via phosh's built-in D-Bus interface. */
  take_screenshot_phosh (app);
}

static void
capture_default_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (user_data);

  /* Keep the app alive while the portal is working */
  g_application_hold (G_APPLICATION (self));

  /* Direct portal call — interactive mode lets the portal handle everything */
  just_capture_screenshot_request_async (
      self->portal,
      0,       /* no specific target */
      TRUE,    /* interactive — portal shows its own UI */
      "",
      NULL,    /* cancellable */
      on_screenshot_done,
      self);
}

static void
capture_screen_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (user_data);
  g_application_hold (G_APPLICATION (self));
  just_capture_screenshot_request_async (
      self->portal, 0, TRUE, "", NULL, on_screenshot_done, self);
}

static void
capture_window_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (user_data);
  g_application_hold (G_APPLICATION (self));
  just_capture_screenshot_request_async (
      self->portal, 0, TRUE, "", NULL, on_screenshot_done, self);
}

static void
capture_area_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (user_data);
  g_application_hold (G_APPLICATION (self));
  just_capture_screenshot_request_async (
      self->portal, 0, TRUE, "", NULL, on_screenshot_done, self);
}

static void
capture_active_window_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (user_data);
  g_application_hold (G_APPLICATION (self));
  just_capture_screenshot_request_async (
      self->portal, 0, TRUE, "", NULL, on_screenshot_done, self);
}

static void
capture_delay_action (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  /* TODO: implement delay */
}

static GActionEntry app_actions[] = {
  { "capture-default",        capture_default_action,        NULL, NULL, NULL },
  { "capture-screen",         capture_screen_action,         NULL, NULL, NULL },
  { "capture-window",         capture_window_action,         NULL, NULL, NULL },
  { "capture-area",           capture_area_action,           NULL, NULL, NULL },
  { "capture-active-window",  capture_active_window_action,  NULL, NULL, NULL },
  { "capture-delay",          capture_delay_action,          "u",  NULL, NULL },
};

static int
just_shot_application_command_line (GApplication            *app,
                                    GApplicationCommandLine *cmdline)
{
  int argc;
  gchar **argv;

  argv = g_application_command_line_get_arguments (cmdline, &argc);

  if (argc > 1 && g_strcmp0 (argv[1], "--version") == 0) {
    g_print ("JustShot version 0.1.0\n");
    g_strfreev (argv);
    return 0;
  }

  g_strfreev (argv);

  /* Take a screenshot via phosh's built-in D-Bus interface.
   * This uses wlr-screencopy directly and works on this device. */
  take_screenshot_phosh (app);
  return 0;
}

static void
just_shot_application_startup (GApplication *app)
{
  JustShotApplication *self = JUST_SHOT_APPLICATION (app);

  G_APPLICATION_CLASS (just_shot_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                    app_actions,
                                    G_N_ELEMENTS (app_actions),
                                    self);

  self->portal = just_capture_portal_get_default ();
}

static void
just_shot_application_finalize (GObject *object)
{
  G_OBJECT_CLASS (just_shot_application_parent_class)->finalize (object);
}

static void
just_shot_application_init (JustShotApplication *self)
{
  self->portal = NULL;
  self->cli_options_parsed = FALSE;
}

static void
just_shot_application_class_init (JustShotApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = just_shot_application_finalize;
  app_class->startup = just_shot_application_startup;
  app_class->activate = just_shot_application_activate;
  app_class->command_line = just_shot_application_command_line;
}

JustShotApplication *
just_shot_application_new (void)
{
  return g_object_new (JUST_SHOT_TYPE_APPLICATION,
                       "application-id", JUST_SHOT_APPLICATION_ID,
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       NULL);
}

CaptureController *
just_shot_application_get_controller (JustShotApplication *app)
{
  return app->controller;
}