hdhomerun_sources = [
  'main.c',
  'hdhomerun-application.c',
  'hdhomerun-window.c',
  'hdhomerun-device-row.c',
  'hdhomerun-tuner-controls.c',
]

cc = meson.get_compiler('c')
hdhomerun_dep = cc.find_library('hdhomerun', required: true)

hdhomerun_deps = [
  dependency('gtk4', version: '>= 4.10'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('glib-2.0', version: '>= 2.76'),
  hdhomerun_dep,
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
