/*
 * Forward declarations for Phosh types used by quick-setting plugins.
 */
#pragma once
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PhoshStatusPage   PhoshStatusPage;
typedef struct _PhoshStatusIcon   PhoshStatusIcon;

/* PhoshQuickSetting */
#define PHOSH_TYPE_QUICK_SETTING (phosh_quick_setting_get_type ())
GType phosh_quick_setting_get_type (void);
#define PHOSH_QUICK_SETTING(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_QUICK_SETTING, PhoshQuickSetting))
#define PHOSH_IS_QUICK_SETTING(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PHOSH_TYPE_QUICK_SETTING))
typedef struct _PhoshQuickSetting PhoshQuickSetting;
typedef struct _PhoshQuickSettingClass PhoshQuickSettingClass;
struct _PhoshQuickSetting { GtkBox parent_instance; };
struct _PhoshQuickSettingClass { GtkBoxClass parent_class; void (*_r[10])(void); };
void phosh_quick_setting_set_active (PhoshQuickSetting *self, gboolean active);
gboolean phosh_quick_setting_get_active (PhoshQuickSetting *self);
void phosh_quick_setting_set_status_icon (PhoshQuickSetting *self, PhoshStatusIcon *status_icon);
PhoshStatusIcon *phosh_quick_setting_get_status_icon (PhoshQuickSetting *self);
void phosh_quick_setting_set_showing_status (PhoshQuickSetting *self, gboolean showing);

/* PhoshStatusIcon */
#define PHOSH_TYPE_STATUS_ICON (phosh_status_icon_get_type ())
GType phosh_status_icon_get_type (void);
#define PHOSH_STATUS_ICON(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_STATUS_ICON, PhoshStatusIcon))
#define PHOSH_IS_STATUS_ICON(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PHOSH_TYPE_STATUS_ICON))
typedef struct _PhoshStatusIcon PhoshStatusIcon;
typedef struct _PhoshStatusIconClass PhoshStatusIconClass;
struct _PhoshStatusIcon { GtkBin parent_instance; };
struct _PhoshStatusIconClass { GtkBinClass parent_class; void (*_r[10])(void); };
void phosh_status_icon_set_icon_name (PhoshStatusIcon *self, const char *icon_name);
void phosh_status_icon_set_info (PhoshStatusIcon *self, const char *info);

/* PhoshStatusPage */
#define PHOSH_TYPE_STATUS_PAGE (phosh_status_page_get_type ())
GType phosh_status_page_get_type (void);
#define PHOSH_STATUS_PAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHOSH_TYPE_STATUS_PAGE, PhoshStatusPage))
#define PHOSH_IS_STATUS_PAGE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PHOSH_TYPE_STATUS_PAGE))
typedef struct _PhoshStatusPage PhoshStatusPage;
typedef struct _PhoshStatusPageClass PhoshStatusPageClass;
struct _PhoshStatusPage { GtkBin parent_instance; };
struct _PhoshStatusPageClass { GtkBinClass parent_class; void (*_r[10])(void); };

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PhoshQuickSetting, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (PhoshStatusIcon, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (PhoshStatusPage, g_object_unref)

G_END_DECLS