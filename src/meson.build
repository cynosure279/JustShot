justshot_sources = files(
  'main.c',
  'application.c',
  'capture-controller.c',
  'document.c',
  'renderer.c',
  'export.c',
  'ui/window.c',
  'ui/preview.c',
  'ui/toolbar.c',
  'ui/clipboard.c',
  'cli/cli.c',
)

m_dep = cc.find_library('m', required: false)
gdkpixbuf_dep = dependency('gdk-pixbuf-2.0')
justshot_deps = [libjustcapture_dep, gtk_dep, adw_dep, gdkpixbuf_dep, m_dep]

executable('justshot', justshot_sources,
  dependencies: justshot_deps,
  install: true,
  win_subsystem: 'windows')

# GSettings schema
install_data('../data/org.just.JustShot.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')

# Desktop file
install_data('../data/justshot.desktop',
  install_dir: get_option('datadir') / 'applications')