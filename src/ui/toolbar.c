/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "toolbar.h"

struct _JustShotToolbar {
  GtkBox          parent_instance;
  ImageDocument  *document;

  GtkWidget      *undo_btn;
  GtkWidget      *redo_btn;
  GtkWidget      *crop_btn;
  GtkWidget      *rotate_btn;
  GtkWidget      *flip_btn;
  GtkWidget      *arrow_btn;
  GtkWidget      *rect_btn;
  GtkWidget      *text_btn;
  GtkWidget      *pen_btn;
  GtkWidget      *blur_btn;
  GtkWidget      *reset_btn;

  gboolean        crop_mode;
  DocumentOpType  draw_mode;
};

G_DEFINE_TYPE (JustShotToolbar, just_shot_toolbar, GTK_TYPE_BOX)

static void
on_undo_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  if (self->document)
    image_document_undo (self->document);
}

static void
on_redo_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  if (self->document)
    image_document_redo (self->document);
}

static void
on_crop_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  self->crop_mode = !self->crop_mode;
  self->draw_mode = DOCUMENT_OP_NONE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->crop_btn), self->crop_mode);
}

static void
on_rotate_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  if (self->document)
    image_document_push_op (self->document, DOCUMENT_OP_ROTATE,
                            rotate_params_new (90));
}

static void
on_flip_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  if (self->document)
    image_document_push_op (self->document, DOCUMENT_OP_FLIP,
                            flip_params_new (TRUE));
}

static void
on_draw_tool_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  DocumentOpType type = GPOINTER_TO_UINT (user_data);

  self->draw_mode = (self->draw_mode == type) ? DOCUMENT_OP_NONE : type;
  self->crop_mode = FALSE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->arrow_btn), self->draw_mode == DOCUMENT_OP_DRAW_ARROW);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->rect_btn), self->draw_mode == DOCUMENT_OP_DRAW_RECTANGLE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->text_btn), self->draw_mode == DOCUMENT_OP_DRAW_TEXT);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->pen_btn), self->draw_mode == DOCUMENT_OP_DRAW_FREEHAND);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->blur_btn), self->draw_mode == DOCUMENT_OP_BLUR);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->crop_btn), FALSE);
}

static void
on_reset_clicked (GtkButton *button, gpointer user_data)
{
  JustShotToolbar *self = JUST_SHOT_TOOLBAR (user_data);
  if (self->document)
    image_document_reset (self->document);
}

static void
on_document_changed (ImageDocument *doc, JustShotToolbar *self)
{
  gtk_widget_set_sensitive (self->undo_btn, image_document_can_undo (doc));
  gtk_widget_set_sensitive (self->redo_btn, image_document_can_redo (doc));
}

static void
just_shot_toolbar_init (JustShotToolbar *self)
{
  GtkWidget *box;

  self->crop_mode = FALSE;
  self->draw_mode = DOCUMENT_OP_NONE;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_margin_start (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self), 6);
  gtk_box_set_spacing (GTK_BOX (self), 4);

  /* Undo/Redo */
  self->undo_btn = gtk_button_new_with_label ("Undo");
  g_signal_connect (self->undo_btn, "clicked", G_CALLBACK (on_undo_clicked), self);
  gtk_box_append (GTK_BOX (self), self->undo_btn);

  self->redo_btn = gtk_button_new_with_label ("Redo");
  g_signal_connect (self->redo_btn, "clicked", G_CALLBACK (on_redo_clicked), self);
  gtk_box_append (GTK_BOX (self), self->redo_btn);

  /* Separator */
  box = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (self), box);

  /* Transform tools */
  self->crop_btn = gtk_toggle_button_new_with_label ("Crop");
  g_signal_connect (self->crop_btn, "clicked", G_CALLBACK (on_crop_clicked), self);
  gtk_box_append (GTK_BOX (self), self->crop_btn);

  self->rotate_btn = gtk_button_new_with_label ("Rotate");
  g_signal_connect (self->rotate_btn, "clicked", G_CALLBACK (on_rotate_clicked), self);
  gtk_box_append (GTK_BOX (self), self->rotate_btn);

  self->flip_btn = gtk_button_new_with_label ("Flip H");
  g_signal_connect (self->flip_btn, "clicked", G_CALLBACK (on_flip_clicked), self);
  gtk_box_append (GTK_BOX (self), self->flip_btn);

  /* Separator */
  box = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (self), box);

  /* Annotation tools */
  self->rect_btn = gtk_toggle_button_new_with_label ("Rect");
  g_signal_connect (self->rect_btn, "clicked", G_CALLBACK (on_draw_tool_clicked), GUINT_TO_POINTER (DOCUMENT_OP_DRAW_RECTANGLE));
  gtk_box_append (GTK_BOX (self), self->rect_btn);

  self->arrow_btn = gtk_toggle_button_new_with_label ("Arrow");
  g_signal_connect (self->arrow_btn, "clicked", G_CALLBACK (on_draw_tool_clicked), GUINT_TO_POINTER (DOCUMENT_OP_DRAW_ARROW));
  gtk_box_append (GTK_BOX (self), self->arrow_btn);

  self->text_btn = gtk_toggle_button_new_with_label ("Text");
  g_signal_connect (self->text_btn, "clicked", G_CALLBACK (on_draw_tool_clicked), GUINT_TO_POINTER (DOCUMENT_OP_DRAW_TEXT));
  gtk_box_append (GTK_BOX (self), self->text_btn);

  self->pen_btn = gtk_toggle_button_new_with_label ("Pen");
  g_signal_connect (self->pen_btn, "clicked", G_CALLBACK (on_draw_tool_clicked), GUINT_TO_POINTER (DOCUMENT_OP_DRAW_FREEHAND));
  gtk_box_append (GTK_BOX (self), self->pen_btn);

  self->blur_btn = gtk_toggle_button_new_with_label ("Blur");
  g_signal_connect (self->blur_btn, "clicked", G_CALLBACK (on_draw_tool_clicked), GUINT_TO_POINTER (DOCUMENT_OP_BLUR));
  gtk_box_append (GTK_BOX (self), self->blur_btn);

  /* Separator */
  box = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (self), box);

  /* Reset */
  self->reset_btn = gtk_button_new_with_label ("Reset");
  g_signal_connect (self->reset_btn, "clicked", G_CALLBACK (on_reset_clicked), self);
  gtk_box_append (GTK_BOX (self), self->reset_btn);

  /* Initially disabled */
  gtk_widget_set_sensitive (self->undo_btn, FALSE);
  gtk_widget_set_sensitive (self->redo_btn, FALSE);
}

static void
just_shot_toolbar_class_init (JustShotToolbarClass *klass)
{
}

GtkWidget *
just_shot_toolbar_new (void)
{
  return g_object_new (JUST_SHOT_TYPE_TOOLBAR, NULL);
}

void
just_shot_toolbar_set_document (JustShotToolbar *toolbar, ImageDocument *doc)
{
  g_return_if_fail (JUST_SHOT_IS_TOOLBAR (toolbar));

  if (toolbar->document) {
    g_signal_handlers_disconnect_by_func (toolbar->document,
                                           G_CALLBACK (on_document_changed),
                                           toolbar);
  }

  toolbar->document = doc;

  if (doc) {
    g_signal_connect (doc, "changed", G_CALLBACK (on_document_changed), toolbar);
    gtk_widget_set_sensitive (toolbar->undo_btn, image_document_can_undo (doc));
    gtk_widget_set_sensitive (toolbar->redo_btn, image_document_can_redo (doc));
  }
}

void
just_shot_toolbar_set_crop_mode (JustShotToolbar *toolbar, gboolean active)
{
  g_return_if_fail (JUST_SHOT_IS_TOOLBAR (toolbar));
  toolbar->crop_mode = active;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->crop_btn), active);
}

void
just_shot_toolbar_set_draw_mode (JustShotToolbar *toolbar, DocumentOpType draw_type)
{
  g_return_if_fail (JUST_SHOT_IS_TOOLBAR (toolbar));
  toolbar->draw_mode = draw_type;
}
