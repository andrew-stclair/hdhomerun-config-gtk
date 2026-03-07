project('libhdhomerun', 'c',
  version: '1.0',
  default_options: [ 'warning_level=2', 'c_std=c11' ],
)

add_project_arguments('-D_GNU_SOURCE', language: 'c')

libhdhomerun_sources = [
  'hdhomerun_channels.c',
  'hdhomerun_channelscan.c',
  'hdhomerun_control.c',
  'hdhomerun_debug.c',
  'hdhomerun_device.c',
  'hdhomerun_device_selector.c',
  'hdhomerun_discover.c',
  'hdhomerun_os_posix.c',
  'hdhomerun_pkt.c',
  'hdhomerun_sock.c',
  'hdhomerun_sock_posix.c',
  'hdhomerun_video.c',
]

if host_machine.system() == 'linux'
  libhdhomerun_sources += 'hdhomerun_sock_netlink.c'
elif host_machine.system() == 'windows'
  libhdhomerun_sources += 'hdhomerun_sock_netdevice.c'
  libhdhomerun_sources += 'hdhomerun_os_windows.c'
  libhdhomerun_sources += 'hdhomerun_sock_windows.c'
else
  libhdhomerun_sources += 'hdhomerun_sock_getifaddrs.c'
endif

libhdhomerun_deps = [
  dependency('threads'),
]

if host_machine.system() == 'linux'
  libhdhomerun_deps += meson.get_compiler('c').find_library('rt')
endif

libhdhomerun = static_library('hdhomerun',
  libhdhomerun_sources,
  dependencies: libhdhomerun_deps,
  include_directories: include_directories('.'),
  install: false,
)

libhdhomerun_dep = declare_dependency(
  link_with: libhdhomerun,
  include_directories: include_directories('.'),
)
