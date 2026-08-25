/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "renderer.h"
#include <math.h>
#include <string.h>
#include <cairo.h>

/* Apply a simple box blur to a surface region */
static void
apply_box_blur (cairo_surface_t *surface,
                gint x, gint y, gint w, gint h,
                gdouble radius)
{
  gint stride, src_width, src_height;
  gint channels = 4;
  gint r = (gint) ceil (radius);
  gint box_size = 2 * r + 1;
  guint8 *data, *copy;
  gint i, j, k, l;

  if (r < 1) return;

  stride = cairo_image_surface_get_stride (surface);
  src_width = cairo_image_surface_get_width (surface);
  src_height = cairo_image_surface_get_height (surface);

  /* Clamp region */
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > src_width) w = src_width - x;
  if (y + h > src_height) h = src_height - y;
  if (w <= 0 || h <= 0) return;

  cairo_surface_flush (surface);
  data = cairo_image_surface_get_data (surface);

  /* Make a copy of the region */
  copy = g_malloc (h * stride);
  for (i = 0; i < h; i++)
    memcpy (copy + i * stride, data + (y + i) * stride + x * channels, w * 4);

  /* Horizontal blur */
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      gint r_acc = 0, g_acc = 0, b_acc = 0, a_acc = 0, count = 0;
      for (k = -r; k <= r; k++) {
        gint sx = i + k;
        if (sx < 0 || sx >= w) continue;
        gint off = (j * stride) + sx * 4;
        r_acc += copy[off + 0];
        g_acc += copy[off + 1];
        b_acc += copy[off + 2];
        a_acc += copy[off + 3];
        count++;
      }
      if (count > 0) {
        gint off = (j * stride) + (x + i) * 4;
        data[off + 0] = r_acc / count;
        data[off + 1] = g_acc / count;
        data[off + 2] = b_acc / count;
        data[off + 3] = a_acc / count;
      }
    }
  }

  /* Refresh copy for vertical pass */
  for (i = 0; i < h; i++)
    memcpy (copy + i * stride, data + (y + i) * stride + x * channels, w * 4);

  /* Vertical blur */
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      gint r_acc = 0, g_acc = 0, b_acc = 0, a_acc = 0, count = 0;
      for (k = -r; k <= r; k++) {
        gint sy = j + k;
        if (sy < 0 || sy >= h) continue;
        gint off = (sy * stride) + i * 4;
        r_acc += copy[off + 0];
        g_acc += copy[off + 1];
        b_acc += copy[off + 2];
        a_acc += copy[off + 3];
        count++;
      }
      if (count > 0) {
        gint off = (j * stride) + (x + i) * 4;
        data[off + 0] = r_acc / count;
        data[off + 1] = g_acc / count;
        data[off + 2] = b_acc / count;
        data[off + 3] = a_acc / count;
      }
    }
  }

  g_free (copy);
  cairo_surface_mark_dirty_rectangle (surface, x, y, w, h);
}

/* Apply color adjustment to a surface */
static void
apply_color_adjust (cairo_surface_t *surface,
                    gdouble brightness,
                    gdouble contrast,
                    gdouble saturation)
{
  gint stride, width, height;
  guint8 *data;
  gint i, j;

  stride = cairo_image_surface_get_stride (surface);
  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);

  cairo_surface_flush (surface);
  data = cairo_image_surface_get_data (surface);

  for (j = 0; j < height; j++) {
    for (i = 0; i < width; i++) {
      gint off = j * stride + i * 4;
      gdouble r = data[off + 0] / 255.0;
      gdouble g = data[off + 1] / 255.0;
      gdouble b = data[off + 2] / 255.0;

      /* Brightness */
      r += brightness;
      g += brightness;
      b += brightness;

      /* Contrast */
      r = (r - 0.5) * contrast + 0.5;
      g = (g - 0.5) * contrast + 0.5;
      b = (b - 0.5) * contrast + 0.5;

      /* Saturation */
      gdouble gray = 0.299 * r + 0.587 * g + 0.114 * b;
      r = gray + saturation * (r - gray);
      g = gray + saturation * (g - gray);
      b = gray + saturation * (b - gray);

      data[off + 0] = (guint8) CLAMP (r * 255.0, 0, 255);
      data[off + 1] = (guint8) CLAMP (g * 255.0, 0, 255);
      data[off + 2] = (guint8) CLAMP (b * 255.0, 0, 255);
    }
  }

  cairo_surface_mark_dirty (surface);
}

/* Draw an arrow head on a cairo context */
static void
draw_arrow_head (cairo_t *cr, gdouble x, gdouble y, gdouble angle, gdouble size)
{
  cairo_save (cr);
  cairo_translate (cr, x, y);
  cairo_rotate (cr, angle);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, -size, -size / 3);
  cairo_line_to (cr, -size, size / 3);
  cairo_close_path (cr);
  cairo_fill (cr);
  cairo_restore (cr);
}

/* Apply a single operation to a cairo context */
static gboolean
apply_operation (cairo_surface_t *surface, DocumentOperation *op, GError **error)
{
  cairo_t *cr;
  gint w, h;

  w = cairo_image_surface_get_width (surface);
  h = cairo_image_surface_get_height (surface);

  cr = cairo_create (surface);

  switch (op->type) {
    case DOCUMENT_OP_CROP: {
      gint x, y, cw, ch;
      cairo_surface_t *cropped;

      g_variant_get (op->params, "(iiii)", &x, &y, &cw, &ch);
      cairo_destroy (cr);

      cropped = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, cw, ch);
      cr = cairo_create (cropped);
      cairo_set_source_surface (cr, surface, -x, -y);
      cairo_paint (cr);
      cairo_destroy (cr);

      /* Replace surface contents */
      {
        guint8 *src_data = cairo_image_surface_get_data (cropped);
        guint8 *dst_data = cairo_image_surface_get_data (surface);
        gint src_stride = cairo_image_surface_get_stride (cropped);
        gint dst_stride = cairo_image_surface_get_stride (surface);
        gint copy_h = MIN (ch, h);
        gint copy_w = MIN (cw, w);

        for (gint row = 0; row < copy_h; row++) {
          memcpy (dst_data + row * dst_stride,
                  src_data + row * src_stride,
                  copy_w * 4);
        }
      }
      cairo_surface_mark_dirty (surface);
      cairo_surface_destroy (cropped);
      return TRUE;
    }

    case DOCUMENT_OP_ROTATE: {
      guint angle;
      g_variant_get (op->params, "(u)", &angle);
      cairo_destroy (cr);

      gint new_w, new_h;
      if (angle == 90 || angle == 270) {
        new_w = h;
        new_h = w;
      } else {
        new_w = w;
        new_h = h;
      }

      cairo_surface_t *rotated = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, new_w, new_h);
      cr = cairo_create (rotated);
      cairo_translate (cr, new_w / 2.0, new_h / 2.0);
      cairo_rotate (cr, angle * G_PI / 180.0);
      cairo_translate (cr, -w / 2.0, -h / 2.0);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      /* Replace surface */
      {
        guint8 *src_data = cairo_image_surface_get_data (rotated);
        guint8 *dst_data = cairo_image_surface_get_data (surface);
        gint src_stride = cairo_image_surface_get_stride (rotated);
        gint dst_stride = cairo_image_surface_get_stride (surface);
        gint copy_h = MIN (new_h, h);
        gint copy_w = MIN (new_w, w);

        for (gint row = 0; row < copy_h; row++) {
          memcpy (dst_data + row * dst_stride,
                  src_data + row * src_stride,
                  copy_w * 4);
        }
      }
      cairo_surface_mark_dirty (surface);
      cairo_surface_destroy (rotated);
      return TRUE;
    }

    case DOCUMENT_OP_FLIP: {
      gboolean horizontal;
      g_variant_get (op->params, "(b)", &horizontal);

      cairo_surface_t *flipped = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
      cairo_t *cr2 = cairo_create (flipped);

      if (horizontal) {
        cairo_scale (cr2, -1, 1);
        cairo_translate (cr2, -w, 0);
      } else {
        cairo_scale (cr2, 1, -1);
        cairo_translate (cr2, 0, -h);
      }

      cairo_set_source_surface (cr2, surface, 0, 0);
      cairo_paint (cr2);
      cairo_destroy (cr2);

      /* Copy back */
      {
        guint8 *src_data = cairo_image_surface_get_data (flipped);
        guint8 *dst_data = cairo_image_surface_get_data (surface);
        gint stride = cairo_image_surface_get_stride (flipped);
        for (gint row = 0; row < h; row++)
          memcpy (dst_data + row * stride, src_data + row * stride, w * 4);
      }
      cairo_surface_mark_dirty (surface);
      cairo_surface_destroy (flipped);
      return TRUE;
    }

    case DOCUMENT_OP_DRAW_RECTANGLE: {
      gdouble x1, y1, x2, y2, sw, sr, sg, sb, sa, fr, fg, fb, fa;
      g_variant_get (op->params, "(ddddddddddddddd)", &x1, &y1, &x2, &y2,
                     &sw, &sr, &sg, &sb, &sa, &fr, &fg, &fb, &fa);

      /* Fill */
      if (fa > 0.0) {
        cairo_set_source_rgba (cr, fr, fg, fb, fa);
        cairo_rectangle (cr, MIN (x1, x2), MIN (y1, y2),
                         fabs (x2 - x1), fabs (y2 - y1));
        cairo_fill (cr);
      }

      /* Stroke */
      cairo_set_source_rgba (cr, sr, sg, sb, sa);
      cairo_set_line_width (cr, sw);
      cairo_rectangle (cr, MIN (x1, x2), MIN (y1, y2),
                       fabs (x2 - x1), fabs (y2 - y1));
      cairo_stroke (cr);
      return TRUE;
    }

    case DOCUMENT_OP_DRAW_ELLIPSE: {
      gdouble cx, cy, rx, ry, sw, sr, sg, sb, sa, fr, fg, fb, fa;
      g_variant_get (op->params, "(ddddddddddddddd)", &cx, &cy, &rx, &ry,
                     &sw, &sr, &sg, &sb, &sa, &fr, &fg, &fb, &fa);

      cairo_save (cr);
      cairo_translate (cr, cx, cy);
      cairo_scale (cr, rx, ry);

      if (fa > 0.0) {
        cairo_set_source_rgba (cr, fr, fg, fb, fa);
        cairo_arc (cr, 0, 0, 1, 0, 2 * G_PI);
        cairo_fill (cr);
      }

      cairo_set_source_rgba (cr, sr, sg, sb, sa);
      cairo_set_line_width (cr, sw / MIN (rx, ry));
      cairo_arc (cr, 0, 0, 1, 0, 2 * G_PI);
      cairo_stroke (cr);
      cairo_restore (cr);
      return TRUE;
    }

    case DOCUMENT_OP_DRAW_ARROW: {
      gdouble x1, y1, x2, y2, sw, r, g, b, a;
      g_variant_get (op->params, "(dddddddddd)", &x1, &y1, &x2, &y2,
                     &sw, &r, &g, &b, &a);

      cairo_set_source_rgba (cr, r, g, b, a);
      cairo_set_line_width (cr, sw);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

      cairo_move_to (cr, x1, y1);
      cairo_line_to (cr, x2, y2);
      cairo_stroke (cr);

      /* Arrow head */
      gdouble angle = atan2 (y2 - y1, x2 - x1);
      draw_arrow_head (cr, x2, y2, angle, sw * 4);
      return TRUE;
    }

    case DOCUMENT_OP_DRAW_TEXT: {
      gdouble x, y, font_size, r, g, b, a;
      gchar *text, *font_family;

      g_variant_get (op->params, "(ddssdddd)", &x, &y, &text, &font_family,
                     &font_size, &r, &g, &b, &a);

      cairo_set_source_rgba (cr, r, g, b, a);
      cairo_select_font_face (cr, font_family,
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
      cairo_set_font_size (cr, font_size);
      cairo_move_to (cr, x, y);
      cairo_show_text (cr, text);

      g_free (text);
      g_free (font_family);
      return TRUE;
    }

    case DOCUMENT_OP_DRAW_FREEHAND: {
      gdouble sw, r, g, b, a;
      GVariant *points;
      GVariantIter iter;
      gdouble px, py;

      g_variant_get (op->params, "(@a(dd)dddd)", &points, &sw, &r, &g, &b, &a);

      cairo_set_source_rgba (cr, r, g, b, a);
      cairo_set_line_width (cr, sw);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);

      g_variant_iter_init (&iter, points);
      if (g_variant_iter_next (&iter, "(dd)", &px, &py)) {
        cairo_move_to (cr, px, py);
        while (g_variant_iter_next (&iter, "(dd)", &px, &py))
          cairo_line_to (cr, px, py);
        cairo_stroke (cr);
      }

      g_variant_unref (points);
      return TRUE;
    }

    case DOCUMENT_OP_BLUR: {
      gdouble bx, by, bw, bh, radius;
      g_variant_get (op->params, "(ddddd)", &bx, &by, &bw, &bh, &radius);

      cairo_destroy (cr);
      apply_box_blur (surface, (gint) bx, (gint) by,
                      (gint) bw, (gint) bh, radius);
      return TRUE;
    }

    case DOCUMENT_OP_ADJUST_COLOR: {
      gdouble brightness, contrast, saturation;
      g_variant_get (op->params, "(ddd)", &brightness, &contrast, &saturation);

      cairo_destroy (cr);
      apply_color_adjust (surface, brightness, contrast, saturation);
      return TRUE;
    }

    case DOCUMENT_OP_NONE:
      cairo_destroy (cr);
      return TRUE;

    default:
      cairo_destroy (cr);
      return TRUE;
  }

  cairo_destroy (cr);
  return TRUE;
}

cairo_surface_t *
renderer_render (ImageDocument *doc, GError **error)
{
  return renderer_render_from (doc, doc->ops, error);
}

cairo_surface_t *
renderer_render_from (ImageDocument *doc, GList *start_op, GError **error)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  gint w, h;
  GList *l;

  g_return_val_if_fail (doc != NULL, NULL);
  g_return_val_if_fail (doc->original_surface != NULL, NULL);

  w = cairo_image_surface_get_width (doc->original_surface);
  h = cairo_image_surface_get_height (doc->original_surface);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cr = cairo_create (surface);
  cairo_set_source_surface (cr, doc->original_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Apply operations up to current */
  for (l = start_op; l; l = l->next) {
    DocumentOperation *op = l->data;
    if (!apply_operation (surface, op, error))
      break;
    if (l == doc->current)
      break;
  }

  return surface;
}
