hdhomerun_sources = [
  'main.c',
  'hdhomerun-application.c',
  'hdhomerun-window.c',
  'hdhomerun-device-row.c',
  'hdhomerun-tuner-controls.c',
]

hdhomerun_deps = [
  dependency('gtk4', version: '>= 4.10'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('glib-2.0', version: '>= 2.76'),
  dependency('hdhomerun'),
  dependency('vlc', required: false),
]

hdhomerun_sources += gnome.compile_resources('hdhomerun-resources',
  'hdhomerun.gresource.xml',
  c_name: 'hdhomerun'
)

executable('hdhomerun-config-gtk', hdhomerun_sources,
  dependencies: hdhomerun_deps,
  install: true,
)
