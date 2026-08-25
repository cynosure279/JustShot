/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <glib.h>
#include "../src/document.h"
#include "../src/export.h"

static void
test_export_png (void)
{
  ImageDocument *doc = image_document_new_from_surface (
      cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10));
  g_assert_nonnull (doc);
  g_object_unref (doc);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/export/png", test_export_png);
  return g_test_run ();
}
