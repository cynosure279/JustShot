test_sources = files(
  '../src/document.c',
  '../src/renderer.c',
  '../src/export.c',
  '../src/ui/clipboard.c',
)

test_document = executable('test-document',
  'test-document.c', test_sources,
  dependencies: [libjustcapture_dep, gtk_dep, adw_dep],
)
test('test-document', test_document)

test_renderer = executable('test-renderer',
  'test-renderer.c', test_sources,
  dependencies: [libjustcapture_dep, gtk_dep, adw_dep],
)
test('test-renderer', test_renderer)

test_export = executable('test-export',
  'test-export.c', test_sources,
  dependencies: [libjustcapture_dep, gtk_dep, adw_dep],
)
test('test-export', test_export)
