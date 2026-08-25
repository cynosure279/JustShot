/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "clipboard.h"

void
clipboard_copy_surface (cairo_surface_t *surface)
{
  GdkClipboard *clipboard;
  GdkContentProvider *provider;
  GdkPixbuf *pixbuf;
  gint w, h;

  g_return_if_fail (surface != NULL);

  w = cairo_image_surface_get_width (surface);
  h = cairo_image_surface_get_height (surface);

  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, w, h);
  if (!pixbuf) {
    g_warning ("Failed to create pixbuf from surface for clipboard");
    return;
  }

  provider = gdk_content_provider_new_typed (GDK_TYPE_PIXBUF, pixbuf);
  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_content (clipboard, provider);

  g_object_unref (pixbuf);
  g_object_unref (provider);
}

gboolean
clipboard_has_image (void)
{
  GdkClipboard *clipboard;

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  return gdk_clipboard_get_content (clipboard) != NULL;
}
