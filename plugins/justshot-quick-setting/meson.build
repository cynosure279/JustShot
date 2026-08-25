gnome = import('gnome')

gio_dep = dependency('gio-2.0')
gtk_dep = dependency('gtk+-3.0')
phosh_plugins_dep = dependency('phosh-plugins')
libphosh_dep = dependency('libphosh-0.45')

prefix = get_option('prefix')
libdir = prefix / get_option('libdir')
plugins_dir = libdir / 'phosh' / 'plugins'
phosh_plugin_icon_dir = get_option('prefix') / get_option('datadir') / 'phosh' / 'plugins' / 'icons'

name = 'justshot-quick-setting'

resources = gnome.compile_resources('phosh-plugin-justshot-resources',
  'phosh-plugin-justshot-quick-setting.gresources.xml',
  c_name: 'phosh_plugin_justshot',
)

shared_module('phosh-plugin-justshot',
  files('phosh-plugin-justshot-quick-setting.c', 'justshot-quick-setting.c'),
  resources,
  c_args: [
    '-DG_LOG_DOMAIN="phosh-plugin-justshot"',
    '-DPLUGIN_NAME="justshot-quick-setting"',
  ],
  dependencies: [gio_dep, gtk_dep, phosh_plugins_dep, libphosh_dep],
  install: true,
  install_dir: plugins_dir,
)

pluginconf = configuration_data()
pluginconf.set('name', name)
pluginconf.set('plugins_dir', plugins_dir)

configure_file(
  input: name + '.desktop.in.in',
  output: name + '.plugin',
  configuration: pluginconf,
  install: true,
  install_dir: plugins_dir,
)
