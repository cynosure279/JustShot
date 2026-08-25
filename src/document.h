/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_DOCUMENT_H
#define JUST_SHOT_DOCUMENT_H

#include <gtk/gtk.h>
#include <cairo.h>

G_BEGIN_DECLS

typedef enum {
  DOCUMENT_OP_NONE,
  DOCUMENT_OP_CROP,
  DOCUMENT_OP_ROTATE,
  DOCUMENT_OP_FLIP,
  DOCUMENT_OP_DRAW_RECTANGLE,
  DOCUMENT_OP_DRAW_ELLIPSE,
  DOCUMENT_OP_DRAW_ARROW,
  DOCUMENT_OP_DRAW_TEXT,
  DOCUMENT_OP_DRAW_FREEHAND,
  DOCUMENT_OP_BLUR,
  DOCUMENT_OP_ADJUST_COLOR,
} DocumentOpType;

typedef struct {
  DocumentOpType type;
  GVariant      *params;
  gint64         timestamp_us;
} DocumentOperation;

typedef struct _ImageDocument {
  cairo_surface_t *original_surface;
  gchar           *original_file_path;
  gint             original_width;
  gint             original_height;

  GList           *ops;
  GList           *saved_pos;
  GList           *current;

  cairo_surface_t *cached_result;
  gboolean         cache_dirty;
} ImageDocument;

void image_document_free (ImageDocument *doc);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ImageDocument, image_document_free)

ImageDocument *image_document_new_from_file (const gchar *file_path, GError **error);
ImageDocument *image_document_new_from_data (gconstpointer data, gsize len, GError **error);
ImageDocument *image_document_new_from_surface (cairo_surface_t *surface);

void image_document_push_op (ImageDocument *doc, DocumentOpType type, GVariant *params);
void image_document_undo (ImageDocument *doc);
void image_document_redo (ImageDocument *doc);
gboolean image_document_can_undo (ImageDocument *doc);
gboolean image_document_can_redo (ImageDocument *doc);
void image_document_reset (ImageDocument *doc);

cairo_surface_t *image_document_render (ImageDocument *doc, GError **error);

void image_document_export_png_async (
    ImageDocument     *doc,
    const gchar       *output_path,
    GCancellable      *cancellable,
    GAsyncReadyCallback callback,
    gpointer           user_data);

gboolean image_document_export_png_finish (
    ImageDocument *doc G_GNUC_UNUSED,
    GAsyncResult  *result,
    GError       **error);

gboolean image_document_is_dirty (ImageDocument *doc);
void image_document_mark_saved (ImageDocument *doc);

gint image_document_get_width (ImageDocument *doc);
gint image_document_get_height (ImageDocument *doc);

/* Operation parameter helpers */
GVariant *crop_params_new (gint x, gint y, gint width, gint height);
GVariant *rotate_params_new (guint angle);
GVariant *flip_params_new (gboolean horizontal);
GVariant *rectangle_params_new (gdouble x1, gdouble y1, gdouble x2, gdouble y2,
                                gdouble stroke_width,
                                gdouble sr, gdouble sg, gdouble sb, gdouble sa,
                                gdouble fr, gdouble fg, gdouble fb, gdouble fa);
GVariant *ellipse_params_new (gdouble cx, gdouble cy, gdouble rx, gdouble ry,
                              gdouble stroke_width,
                              gdouble sr, gdouble sg, gdouble sb, gdouble sa,
                              gdouble fr, gdouble fg, gdouble fb, gdouble fa);
GVariant *arrow_params_new (gdouble x1, gdouble y1, gdouble x2, gdouble y2,
                            gdouble stroke_width,
                            gdouble r, gdouble g, gdouble b, gdouble a);
GVariant *text_params_new (gdouble x, gdouble y, const gchar *text,
                           const gchar *font_family, gdouble font_size,
                           gdouble r, gdouble g, gdouble b, gdouble a);
GVariant *freehand_params_new (GVariant *points, gdouble stroke_width,
                               gdouble r, gdouble g, gdouble b, gdouble a);
GVariant *blur_params_new (gdouble x, gdouble y, gdouble width, gdouble height, gdouble radius);
GVariant *color_adjust_params_new (gdouble brightness, gdouble contrast, gdouble saturation);

G_END_DECLS

#endif /* JUST_SHOT_DOCUMENT_H */
