project('justshot', 'c',
  version: '0.1.0',
  meson_version: '>= 1.3.0',
  license: 'GPL-2.0+',
  default_options: ['warning_level=2', 'c_std=c17'])

add_project_arguments('-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_80',
                      '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_80',
                      '-DG_LOG_DOMAIN="JustShot"',
                      language: 'c')

# Dependencies
libjustcapture_dep = dependency('justcapture', required: false, version: '>= 0.1.0')
if not libjustcapture_dep.found()
  libjustcapture_dep = subproject('libjustcapture').get_variable('libjustcapture_dep')
endif

gtk_dep = dependency('gtk4', version: '>= 4.14')
gtk3_dep = dependency('gtk+-3.0', required: false)
adw_dep = dependency('libadwaita-1', version: '>= 1.6')

cc = meson.get_compiler('c')
subdir('src/')

# The phosh plugin is built only when explicitly enabled and phosh deps are available
if get_option('phosh-plugin')
  subdir('plugins/justshot-quick-setting')
endif

if get_option('tests')
  subdir('tests/')
endif