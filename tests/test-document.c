/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <glib.h>
#include "../src/document.h"

static void
test_document_create (void)
{
  ImageDocument *doc = image_document_new_from_surface (
      cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100));
  g_assert_nonnull (doc);
  g_assert_true (image_document_get_width (doc) == 100);
  g_assert_true (image_document_get_height (doc) == 100);
  g_assert_false (image_document_can_undo (doc));
  g_assert_false (image_document_can_redo (doc));
  g_object_unref (doc);
}

static void
test_document_undo_redo (void)
{
  ImageDocument *doc = image_document_new_from_surface (
      cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100));
  g_assert_nonnull (doc);

  image_document_push_op (doc, DOCUMENT_OP_ROTATE, rotate_params_new (90));
  g_assert_true (image_document_can_undo (doc));
  g_assert_false (image_document_can_redo (doc));

  image_document_undo (doc);
  g_assert_false (image_document_can_undo (doc));
  g_assert_true (image_document_can_redo (doc));

  image_document_redo (doc);
  g_assert_true (image_document_can_undo (doc));
  g_assert_false (image_document_can_redo (doc));

  g_object_unref (doc);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/document/create", test_document_create);
  g_test_add_func ("/document/undo-redo", test_document_undo_redo);
  return g_test_run ();
}
