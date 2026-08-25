/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef JUST_SHOT_CLI_H
#define JUST_SHOT_CLI_H

#include <gtk/gtk.h>
#include <adwaita.h>

typedef struct {
  gint     target;
  guint    delay_seconds;
  gchar   *output_path;
  gboolean clipboard;
  gboolean interactive;
  gboolean quiet;
  gboolean version;
} CliOptions;

gboolean cli_parse_args (GApplication *app, int argc, char *argv[], CliOptions *opts);
void cli_execute (GApplication *app, CliOptions *opts);
void cli_options_free (CliOptions *opts);

#endif /* JUST_SHOT_CLI_H */
