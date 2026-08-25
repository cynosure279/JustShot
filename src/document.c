/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "document.h"
#include "renderer.h"
#include "export.h"

static void
image_document_emit_changed (ImageDocument *doc)
{
  doc->cache_dirty = TRUE;
}

void
image_document_free (ImageDocument *doc)
{
  if (doc == NULL)
    return;

  for (GList *l = doc->ops; l; l = l->next) {
    DocumentOperation *op = l->data;
    if (op->params) g_variant_unref (op->params);
    g_free (op);
  }
  g_list_free (doc->ops);

  g_clear_pointer (&doc->original_surface, cairo_surface_destroy);
  g_clear_pointer (&doc->cached_result, cairo_surface_destroy);
  g_free (doc->original_file_path);
  g_free (doc);
}

ImageDocument *
image_document_new_from_file (const gchar *file_path, GError **error)
{
  g_return_val_if_fail (file_path != NULL, NULL);

  cairo_surface_t *surface = cairo_image_surface_create_from_png (file_path);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                 "Failed to load image: %s", file_path);
    cairo_surface_destroy (surface);
    return NULL;
  }

  ImageDocument *doc = g_new0 (ImageDocument, 1);
  doc->original_surface = surface;
  doc->original_file_path = g_strdup (file_path);
  doc->original_width = cairo_image_surface_get_width (surface);
  doc->original_height = cairo_image_surface_get_height (surface);
  doc->cache_dirty = TRUE;
  return doc;
}

ImageDocument *
image_document_new_from_data (gconstpointer data, gsize len, GError **error)
{
  GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
  if (!gdk_pixbuf_loader_write (loader, data, len, error) ||
      !gdk_pixbuf_loader_close (loader, error)) {
    g_object_unref (loader);
    return NULL;
  }

  GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (!pixbuf) {
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                 "Failed to decode image data");
    g_object_unref (loader);
    return NULL;
  }

  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                          gdk_pixbuf_get_width (pixbuf),
                                                          gdk_pixbuf_get_height (pixbuf));
  {
    cairo_t *cr = cairo_create (surface);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);
  }
  g_object_unref (loader);

  ImageDocument *doc = g_new0 (ImageDocument, 1);
  doc->original_surface = surface;
  doc->original_width = cairo_image_surface_get_width (surface);
  doc->original_height = cairo_image_surface_get_height (surface);
  doc->cache_dirty = TRUE;
  return doc;
}

ImageDocument *
image_document_new_from_surface (cairo_surface_t *surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  ImageDocument *doc = g_new0 (ImageDocument, 1);
  doc->original_surface = cairo_surface_reference (surface);
  doc->original_width = cairo_image_surface_get_width (surface);
  doc->original_height = cairo_image_surface_get_height (surface);
  doc->cache_dirty = TRUE;
  return doc;
}

static DocumentOperation *
document_operation_new (DocumentOpType type, GVariant *params)
{
  DocumentOperation *op = g_new0 (DocumentOperation, 1);
  op->type = type;
  op->params = params ? g_variant_ref_sink (params) : NULL;
  op->timestamp_us = g_get_real_time ();
  return op;
}

void
image_document_push_op (ImageDocument *doc, DocumentOpType type, GVariant *params)
{
  g_return_if_fail (doc != NULL);

  while (doc->current && doc->current->next) {
    GList *next = doc->current->next;
    DocumentOperation *op = next->data;
    if (op->params) g_variant_unref (op->params);
    g_free (op);
    doc->ops = g_list_delete_link (doc->ops, next);
  }

  DocumentOperation *op = document_operation_new (type, params);
  doc->ops = g_list_append (doc->ops, op);
  doc->current = g_list_last (doc->ops);
  image_document_emit_changed (doc);
}

void
image_document_undo (ImageDocument *doc)
{
  g_return_if_fail (doc != NULL);
  if (doc->current)
    doc->current = doc->current->prev;
  image_document_emit_changed (doc);
}

void
image_document_redo (ImageDocument *doc)
{
  g_return_if_fail (doc != NULL);
  if (doc->current && doc->current->next)
    doc->current = doc->current->next;
  else if (!doc->current && doc->ops)
    doc->current = doc->ops;
  image_document_emit_changed (doc);
}

gboolean
image_document_can_undo (ImageDocument *doc)
{
  g_return_val_if_fail (doc != NULL, FALSE);
  return doc->current != NULL;
}

gboolean
image_document_can_redo (ImageDocument *doc)
{
  g_return_val_if_fail (doc != NULL, FALSE);
  if (!doc->current)
    return doc->ops != NULL;
  return doc->current->next != NULL;
}

void
image_document_reset (ImageDocument *doc)
{
  g_return_if_fail (doc != NULL);

  for (GList *l = doc->ops; l; l = l->next) {
    DocumentOperation *op = l->data;
    if (op->params) g_variant_unref (op->params);
    g_free (op);
  }
  g_list_free (doc->ops);
  doc->ops = NULL;
  doc->current = NULL;
  doc->saved_pos = NULL;
  image_document_emit_changed (doc);
}

cairo_surface_t *
image_document_render (ImageDocument *doc, GError **error)
{
  g_return_val_if_fail (doc != NULL, NULL);

  if (doc->cached_result && !doc->cache_dirty)
    return cairo_surface_reference (doc->cached_result);

  g_clear_pointer (&doc->cached_result, cairo_surface_destroy);
  doc->cached_result = renderer_render (doc, error);
  if (doc->cached_result)
    doc->cache_dirty = FALSE;

  return doc->cached_result ? cairo_surface_reference (doc->cached_result) : NULL;
}

void
image_document_export_png_async (ImageDocument     *doc,
                                 const gchar       *output_path,
                                 GCancellable      *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer           user_data)
{
  GError *error = NULL;
  cairo_surface_t *result;

  g_return_if_fail (doc != NULL);

  result = image_document_render (doc, &error);
  if (!result) {
    GTask *task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  export_to_png_async (result, output_path, cancellable, callback, user_data);
  cairo_surface_destroy (result);
}

gboolean
image_document_export_png_finish (ImageDocument *doc G_GNUC_UNUSED,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  return export_to_png_finish (result, error);
}

gboolean
image_document_is_dirty (ImageDocument *doc)
{
  g_return_val_if_fail (doc != NULL, FALSE);
  return doc->saved_pos != doc->current;
}

void
image_document_mark_saved (ImageDocument *doc)
{
  g_return_if_fail (doc != NULL);
  doc->saved_pos = doc->current;
}

gint
image_document_get_width (ImageDocument *doc)
{
  g_return_val_if_fail (doc != NULL, 0);
  return doc->original_width;
}

gint
image_document_get_height (ImageDocument *doc)
{
  g_return_val_if_fail (doc != NULL, 0);
  return doc->original_height;
}

/* Operation parameter helpers */
GVariant *
crop_params_new (gint x, gint y, gint width, gint height)
{ return g_variant_new ("(iiii)", x, y, width, height); }

GVariant *
rotate_params_new (guint angle)
{ return g_variant_new ("(u)", angle); }

GVariant *
flip_params_new (gboolean horizontal)
{ return g_variant_new ("(b)", horizontal); }

GVariant *
rectangle_params_new (gdouble x1, gdouble y1, gdouble x2, gdouble y2,
                       gdouble stroke_width,
                       gdouble sr, gdouble sg, gdouble sb, gdouble sa,
                       gdouble fr, gdouble fg, gdouble fb, gdouble fa)
{ return g_variant_new ("(ddddddddddddd)", x1, y1, x2, y2, stroke_width,
                        sr, sg, sb, sa, fr, fg, fb, fa); }

GVariant *
ellipse_params_new (gdouble cx, gdouble cy, gdouble rx, gdouble ry,
                     gdouble stroke_width,
                     gdouble sr, gdouble sg, gdouble sb, gdouble sa,
                     gdouble fr, gdouble fg, gdouble fb, gdouble fa)
{ return g_variant_new ("(ddddddddddddd)", cx, cy, rx, ry, stroke_width,
                        sr, sg, sb, sa, fr, fg, fb, fa); }

GVariant *
arrow_params_new (gdouble x1, gdouble y1, gdouble x2, gdouble y2,
                   gdouble stroke_width,
                   gdouble r, gdouble g, gdouble b, gdouble a)
{ return g_variant_new ("(dddddddddd)", x1, y1, x2, y2, stroke_width, r, g, b, a); }

GVariant *
text_params_new (gdouble x, gdouble y, const gchar *text,
                 const gchar *font_family, gdouble font_size,
                 gdouble r, gdouble g, gdouble b, gdouble a)
{ return g_variant_new ("(ddssdddd)", x, y, text, font_family, font_size, r, g, b, a); }

GVariant *
freehand_params_new (GVariant *points, gdouble stroke_width,
                      gdouble r, gdouble g, gdouble b, gdouble a)
{
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(dd)ddddd)"));
  g_variant_builder_add_value (&builder, points);
  g_variant_builder_add (&builder, "d", stroke_width);
  g_variant_builder_add (&builder, "d", r);
  g_variant_builder_add (&builder, "d", g);
  g_variant_builder_add (&builder, "d", b);
  g_variant_builder_add (&builder, "d", a);
  return g_variant_builder_end (&builder);
}

GVariant *
blur_params_new (gdouble x, gdouble y, gdouble width, gdouble height, gdouble radius)
{ return g_variant_new ("(ddddd)", x, y, width, height, radius); }

GVariant *
color_adjust_params_new (gdouble brightness, gdouble contrast, gdouble saturation)
{ return g_variant_new ("(ddd)", brightness, contrast, saturation); }
