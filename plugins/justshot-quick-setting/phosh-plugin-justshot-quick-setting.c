/*
 * GIO module entry point for the JustShot Phosh plugin.
 */
#include "phosh-plugin.h"
#include "justshot-quick-setting.h"

char **g_io_phosh_plugin_justshot_quick_setting_query (void);

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));

  g_io_extension_point_implement (
    PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET,
    JUSTSHOT_TYPE_QUICK_SETTING,
    "justshot-quick-setting", 10);
}

void
g_io_module_unload (GIOModule *module)
{
}

char **
g_io_phosh_plugin_justshot_quick_setting_query (void)
{
  char *eps[] = { PHOSH_PLUGIN_EXTENSION_POINT_QUICK_SETTING_WIDGET, NULL };
  return g_strdupv (eps);
}
