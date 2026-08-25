/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <glib.h>
#include "../src/document.h"
#include "../src/renderer.h"

static void
test_renderer_basic (void)
{
  ImageDocument *doc = image_document_new_from_surface (
      cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 50, 50));
  GError *error = NULL;
  cairo_surface_t *result;

  result = image_document_render (doc, &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_true (cairo_image_surface_get_width (result) == 50);
  g_assert_true (cairo_image_surface_get_height (result) == 50);
  cairo_surface_destroy (result);
  g_object_unref (doc);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/renderer/basic", test_renderer_basic);
  return g_test_run ();
}
