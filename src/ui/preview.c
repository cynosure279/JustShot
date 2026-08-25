/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "preview.h"

struct _JustShotPreview {
  GtkDrawingArea   parent_instance;
  ImageDocument   *document;
  cairo_surface_t *current_surface;
  gdouble          zoom;
  gdouble          pan_x;
  gdouble          pan_y;
  gboolean         dragging;
  gdouble          drag_start_x;
  gdouble          drag_start_y;
  gdouble          drag_pan_x;
  gdouble          drag_pan_y;
};

G_DEFINE_TYPE (JustShotPreview, just_shot_preview, GTK_TYPE_DRAWING_AREA)

static void
on_document_changed (ImageDocument *doc, JustShotPreview *preview)
{
  GError *error = NULL;

  g_clear_pointer (&preview->current_surface, cairo_surface_destroy);
  preview->current_surface = image_document_render (doc, &error);
  if (error) {
    g_warning ("Render error: %s", error->message);
    g_error_free (error);
  }

  gtk_widget_queue_draw (GTK_WIDGET (preview));
}

static void
just_shot_preview_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  JustShotPreview *preview = JUST_SHOT_PREVIEW (widget);
  graphene_rect_t bounds;
  gdouble widget_w, widget_h;
  gdouble img_w, img_h;
  gdouble scale;
  cairo_t *cr;

  /* Get widget size */
  widget_w = gtk_widget_get_width (widget);
  widget_h = gtk_widget_get_height (widget);

  if (!preview->current_surface) {
    /* Draw placeholder */
    cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, widget_w, widget_h));
    cairo_set_source_rgba (cr, 0.2, 0.2, 0.2, 1.0);
    cairo_paint (cr);
    cairo_set_source_rgba (cr, 0.7, 0.7, 0.7, 1.0);
    cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (cr, 20);
    cairo_move_to (cr, widget_w / 2 - 50, widget_h / 2);
    cairo_show_text (cr, "No screenshot");
    cairo_destroy (cr);
    return;
  }

  img_w = cairo_image_surface_get_width (preview->current_surface);
  img_h = cairo_image_surface_get_height (preview->current_surface);

  if (img_w == 0 || img_h == 0) return;

  /* Calculate scale to fit */
  scale = preview->zoom * MIN (widget_w / img_w, widget_h / img_h);

  /* Center with pan offset */
  gdouble offset_x = (widget_w - img_w * scale) / 2 + preview->pan_x;
  gdouble offset_y = (widget_h - img_h * scale) / 2 + preview->pan_y;

  cr = gtk_snapshot_append_cairo (snapshot,
    &GRAPHENE_RECT_INIT (0, 0, widget_w, widget_h));

  /* Background */
  cairo_set_source_rgba (cr, 0.15, 0.15, 0.15, 1.0);
  cairo_paint (cr);

  /* Image */
  cairo_translate (cr, offset_x, offset_y);
  cairo_scale (cr, scale, scale);
  cairo_set_source_surface (cr, preview->current_surface, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
}

static gboolean
on_scroll (GtkWidget      *widget,
           gdouble         dx,
           gdouble         dy,
           JustShotPreview *preview)
{
  if (dy > 0)
    just_shot_preview_zoom_out (preview);
  else if (dy < 0)
    just_shot_preview_zoom_in (preview);

  return GDK_EVENT_STOP;
}

static gboolean
on_press (GtkGestureClick *gesture,
          gint             n_press,
          gdouble          x,
          gdouble          y,
          JustShotPreview *preview)
{
  preview->dragging = TRUE;
  preview->drag_start_x = x;
  preview->drag_start_y = y;
  preview->drag_pan_x = preview->pan_x;
  preview->drag_pan_y = preview->pan_y;
  return GDK_EVENT_STOP;
}

static gboolean
on_release (GtkGestureClick *gesture,
            gint             n_press,
            gdouble          x,
            gdouble          y,
            JustShotPreview *preview)
{
  preview->dragging = FALSE;
  return GDK_EVENT_STOP;
}

static gboolean
on_motion (GtkEventControllerMotion *controller,
           gdouble                   x,
           gdouble                   y,
           JustShotPreview          *preview)
{
  if (preview->dragging) {
    preview->pan_x = preview->drag_pan_x + (x - preview->drag_start_x);
    preview->pan_y = preview->drag_pan_y + (y - preview->drag_start_y);
    gtk_widget_queue_draw (GTK_WIDGET (preview));
  }
  return GDK_EVENT_STOP;
}

static void
just_shot_preview_init (JustShotPreview *preview)
{
  GtkGesture *drag, *click;
  GtkEventController *scroll, *motion;

  preview->zoom = 1.0;
  preview->pan_x = 0;
  preview->pan_y = 0;
  preview->dragging = FALSE;
  preview->document = NULL;
  preview->current_surface = NULL;

  /* Scroll zoom */
  scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  g_signal_connect (scroll, "scroll", G_CALLBACK (on_scroll), preview);
  gtk_widget_add_controller (GTK_WIDGET (preview), scroll);

  /* Pan via drag */
  click = GTK_GESTURE (gtk_gesture_click_new ());
  g_signal_connect (click, "pressed", G_CALLBACK (on_press), preview);
  g_signal_connect (click, "released", G_CALLBACK (on_release), preview);
  gtk_widget_add_controller (GTK_WIDGET (preview), GTK_EVENT_CONTROLLER (click));

  motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "motion", G_CALLBACK (on_motion), preview);
  gtk_widget_add_controller (GTK_WIDGET (preview), motion);
}

static void
just_shot_preview_class_init (JustShotPreviewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->snapshot = just_shot_preview_snapshot;
}

GtkWidget *
just_shot_preview_new (void)
{
  return g_object_new (JUST_SHOT_TYPE_PREVIEW, NULL);
}

void
just_shot_preview_set_document (JustShotPreview *preview, ImageDocument *doc)
{
  g_return_if_fail (JUST_SHOT_IS_PREVIEW (preview));

  if (preview->document) {
    g_signal_handlers_disconnect_by_func (preview->document,
                                           G_CALLBACK (on_document_changed),
                                           preview);
  }

  preview->document = doc;

  if (doc) {
    g_signal_connect (doc, "changed", G_CALLBACK (on_document_changed), preview);
    on_document_changed (doc, preview);
  }

  just_shot_preview_zoom_fit (preview);
}

void
just_shot_preview_set_zoom (JustShotPreview *preview, gdouble zoom)
{
  g_return_if_fail (JUST_SHOT_IS_PREVIEW (preview));
  preview->zoom = CLAMP (zoom, 0.1, 10.0);
  gtk_widget_queue_draw (GTK_WIDGET (preview));
}

gdouble
just_shot_preview_get_zoom (JustShotPreview *preview)
{
  g_return_val_if_fail (JUST_SHOT_IS_PREVIEW (preview), 1.0);
  return preview->zoom;
}

void
just_shot_preview_zoom_in (JustShotPreview *preview)
{
  just_shot_preview_set_zoom (preview, preview->zoom * 1.2);
}

void
just_shot_preview_zoom_out (JustShotPreview *preview)
{
  just_shot_preview_set_zoom (preview, preview->zoom / 1.2);
}

void
just_shot_preview_zoom_fit (JustShotPreview *preview)
{
  g_return_if_fail (JUST_SHOT_IS_PREVIEW (preview));
  preview->zoom = 1.0;
  preview->pan_x = 0;
  preview->pan_y = 0;
  gtk_widget_queue_draw (GTK_WIDGET (preview));
}