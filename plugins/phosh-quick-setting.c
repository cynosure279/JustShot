/*
 * Copyright (C) 2026 JustShot Contributors
 * SPDX-License-Identifier: GPL-2.0+
 *
 * Phosh Quick Setting plugin for JustShot.
 *
 * This plugin is ONLY a GDBusActionGroup client.
 * It does NOT:
 *   - Perform any Portal operations
 *   - Save any files
 *   - Edit any images
 *   - Call libjustcapture directly
 */

#include <gio/gio.h>
#include <phosh-plugin.h>

#define JUST_SHOT_BUS_NAME "org.just.JustShot"
#define JUST_SHOT_OBJECT_PATH "/org/just/justshot"

typedef struct {
  PhoshQuickSetting  parent;
  GDBusActionGroup  *action_group;
} JustShotQuickSetting;

typedef struct {
  PhoshQuickSettingClass parent_class;
} JustShotQuickSettingClass;

G_DEFINE_TYPE (JustShotQuickSetting, just_shot_quick_setting, PHOSH_TYPE_QUICK_SETTING)

static void
just_shot_quick_setting_activate (PhoshQuickSetting *setting)
{
  JustShotQuickSetting *self = JUST_SHOT_QUICK_SETTING (setting);

  if (self->action_group) {
    g_dbus_action_group_activate_action (self->action_group,
                                          "capture-default", NULL);
  }
}

static void
just_shot_quick_setting_dispose (GObject *object)
{
  JustShotQuickSetting *self = JUST_SHOT_QUICK_SETTING (object);

  g_clear_object (&self->action_group);

  G_OBJECT_CLASS (just_shot_quick_setting_parent_class)->dispose (object);
}

static void
just_shot_quick_setting_init (JustShotQuickSetting *self)
{
  self->action_group = g_dbus_action_group_get (
      G_BUS_TYPE_SESSION, JUST_SHOT_BUS_NAME, JUST_SHOT_OBJECT_PATH);

  phosh_quick_setting_set_title (PHOSH_QUICK_SETTING (self), "Screenshot");
  phosh_quick_setting_set_icon_name (PHOSH_QUICK_SETTING (self), "camera-photo-symbolic");
}

static void
just_shot_quick_setting_class_init (JustShotQuickSettingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PhoshQuickSettingClass *setting_class = PHOSH_QUICK_SETTING_CLASS (klass);

  object_class->dispose = just_shot_quick_setting_dispose;
  setting_class->activate = just_shot_quick_setting_activate;
}

/* Plugin entry point */
GType
phosh_plugin_get_type (void)
{
  return JUST_SHOT_TYPE_QUICK_SETTING;
}
