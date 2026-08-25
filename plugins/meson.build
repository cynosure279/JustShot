
phosh_dep = dependency('phosh-plugins', required: get_option('phosh-plugin'))

if phosh_dep.found()
  shared_library('justshot-quick-setting',
    'phosh-quick-setting.c',
    dependencies: [phosh_dep, dependency('glib-2.0'), dependency('gio-2.0')],
    install: true,
    install_dir: phosh_dep.get_variable('plugindir'))
endif
