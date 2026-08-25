/*
 * JustShot entry point.
 *
 * Modes:
 *   --area    : Interactive area selection (phosh SelectArea + ScreenshotArea)
 *   default   : Full screen screenshot
 *
 * Options:
 *   --delay N : Wait N seconds before screenshot
 *   --version : Print version
 *
 * Uses phosh's org.gnome.Shell.Screenshot D-Bus interface.
 */
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <justcapture/filename.h>
#include <justcapture/output-path.h>

#define PHOSH_SS_BUS   "org.gnome.Shell.Screenshot"
#define PHOSH_SS_PATH  "/org/gnome/Shell/Screenshot"
#define PHOSH_SS_IFACE "org.gnome.Shell.Screenshot"

static GMainLoop *loop = NULL;
static GDBusConnection *conn = NULL;
static gboolean area_mode = FALSE;
static guint delay_seconds = 0;

static void send_notification (const gchar *summary, const gchar *body)
{
  GDBusConnection *c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (!c) return;
  GVariantBuilder b;
  g_variant_builder_init (&b, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&b, "{sv}", "category", g_variant_new_string ("device"));
  g_dbus_connection_call (c, "org.freedesktop.Notifications",
    "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify",
    g_variant_new ("(susssasa{sv}i)", "JustShot", 0, "camera-photo-symbolic",
      summary, body ? body : "", NULL, &b, -1),
    NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_object_unref (c);
}

static void
save_and_notify (const gchar *filename_used)
{
  g_autofree gchar *basename = just_capture_filename_make_screenshot (NULL);
  g_autofree gchar *dir = just_capture_output_path_get_screenshots_dir ();
  g_autoptr(GError) dir_err = NULL;

  if (!just_capture_output_path_ensure_dir (dir, &dir_err))
    {
      send_notification ("Screenshot Failed", dir_err->message);
      return;
    }

  g_autofree gchar *unique = just_capture_filename_make_unique (dir, basename, ".png", NULL);
  if (!unique) return;

  g_autoptr(GFile) src = g_file_new_for_path (filename_used);
  g_autoptr(GFile) dst = g_file_new_for_path (unique);
  g_autoptr(GError) copy_err = NULL;

  if (g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &copy_err))
    {
      g_message ("Screenshot saved: %s", unique);
      g_autofree gchar *short_name = g_path_get_basename (unique);
      g_autofree gchar *body = g_strdup_printf ("Saved: %s", short_name);
      send_notification ("Screenshot Saved", body);
    }
  else
    {
      send_notification ("Screenshot Failed", copy_err->message);
    }
  g_unlink (filename_used);
}

/* --- Step 3: ScreenshotArea or Screenshot --- */

static void
on_screenshot_done (GObject *source G_GNUC_UNUSED, GAsyncResult *res,
                    gpointer user_data G_GNUC_UNUSED)
{
  GError *error = NULL;
  g_autoptr(GVariant) result = g_dbus_connection_call_finish (
    G_DBUS_CONNECTION (source), res, &error);

  if (result == NULL)
    {
      g_autofree gchar *msg = g_strdup_printf ("Screenshot failed: %s",
        error ? error->message : "unknown");
      g_warning ("%s", msg);
      send_notification ("Screenshot Failed", msg);
      if (error) g_error_free (error);
      g_main_loop_quit (loop);
      return;
    }

  gboolean success = FALSE;
  const gchar *filename_used = NULL;
  g_variant_get (result, "(b&s)", &success, &filename_used);

  if (success && filename_used)
    save_and_notify (filename_used);
  else
    send_notification ("Screenshot Failed", "No screenshot captured");

  g_main_loop_quit (loop);
}

static void
take_screenshot (gint x, gint y, gint w, gint h)
{
  g_autofree gchar *tmp_path = g_strdup_printf ("/tmp/justshot-%u.png", (guint) getpid ());

  if (w > 0 && h > 0)
    {
      /* Area screenshot */
      g_dbus_connection_call (conn, PHOSH_SS_BUS, PHOSH_SS_PATH,
        PHOSH_SS_IFACE, "ScreenshotArea",
        g_variant_new ("(iiiibs)", x, y, w, h, FALSE, tmp_path),
        G_VARIANT_TYPE ("(bs)"), G_DBUS_CALL_FLAGS_NONE, 30000,
        NULL, on_screenshot_done, NULL);
    }
  else
    {
      /* Full screen */
      g_dbus_connection_call (conn, PHOSH_SS_BUS, PHOSH_SS_PATH,
        PHOSH_SS_IFACE, "Screenshot",
        g_variant_new ("(bbs)", TRUE, FALSE, tmp_path),
        G_VARIANT_TYPE ("(bs)"), G_DBUS_CALL_FLAGS_NONE, 15000,
        NULL, on_screenshot_done, NULL);
    }
}

/* --- Step 2: SelectArea callback --- */

static void
on_select_area_done (GObject *source G_GNUC_UNUSED, GAsyncResult *res,
                     gpointer user_data G_GNUC_UNUSED)
{
  GError *error = NULL;
  g_autoptr(GVariant) result = g_dbus_connection_call_finish (
    G_DBUS_CONNECTION (source), res, &error);

  if (result == NULL)
    {
      /* User cancelled area selection */
      g_message ("Area selection cancelled: %s", error ? error->message : "cancelled");
      if (error) g_error_free (error);
      g_main_loop_quit (loop);
      return;
    }

  gint x, y, w, h;
  g_variant_get (result, "(iiii)", &x, &y, &w, &h);
  g_message ("Selected area: %d,%d %dx%d", x, y, w, h);

  if (w <= 0 || h <= 0)
    {
      send_notification ("Screenshot Cancelled", "Selected area too small");
      g_main_loop_quit (loop);
      return;
    }

  take_screenshot (x, y, w, h);
}

/* --- Step 1: Start --- */

static gboolean
do_capture (gpointer user_data G_GNUC_UNUSED)
{
  if (area_mode)
    {
      /* Interactive area selection — phosh shows a selection overlay */
      g_dbus_connection_call (conn, PHOSH_SS_BUS, PHOSH_SS_PATH,
        PHOSH_SS_IFACE, "SelectArea",
        NULL, G_VARIANT_TYPE ("(iiii)"), G_DBUS_CALL_FLAGS_NONE, 60000,
        NULL, on_select_area_done, NULL);
    }
  else
    {
      take_screenshot (0, 0, 0, 0);
    }

  return G_SOURCE_REMOVE;
}

int
main (int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
    {
      if (g_strcmp0 (argv[i], "--version") == 0 || g_strcmp0 (argv[i], "-v") == 0)
        {
          g_print ("JustShot version 0.1.0\n");
          return 0;
        }
      if (g_strcmp0 (argv[i], "--delay") == 0 && i + 1 < argc)
        delay_seconds = (guint) g_ascii_strtoull (argv[++i], NULL, 10);
      if (g_strcmp0 (argv[i], "--area") == 0)
        area_mode = TRUE;
    }

  GError *error = NULL;
  conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (conn == NULL)
    {
      g_warning ("Cannot connect to session bus: %s", error->message);
      g_error_free (error);
      return 1;
    }

  loop = g_main_loop_new (NULL, FALSE);

  /* Delay before screenshot (lets the quick settings panel fold) */
  if (delay_seconds > 0)
    {
      g_message ("Waiting %u seconds...", delay_seconds);
      g_timeout_add_seconds (delay_seconds, do_capture, NULL);
    }
  else
    {
      do_capture (NULL);
    }

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (conn);
  return 0;
}
