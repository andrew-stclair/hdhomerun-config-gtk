app_sources = [
  'main.c',
  'hdhomerun-app.c',
  'hdhomerun-window.c',
  'hdhomerun-device-list.c',
  'hdhomerun-device-view.c',
]

app_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  libhdhomerun_dep,
]

resources = gnome.compile_resources('hdhomerun-resources',
  'hdhomerun.gresource.xml',
  source_dir: '.',
)

executable('hdhomerun-config-gtk',
  app_sources,
  resources,
  dependencies: app_deps,
  install: true,
)
