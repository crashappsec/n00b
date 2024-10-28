project(
  'linuxheaders',
  'c',
  version : '4.19.88',
  default_options: [],
  meson_version : '>=1.5.0',
)

i = run_command(
  'sh',
  '-exc',
  '''
[ -d include ] && exit 0
make ARCH=$(uname -m) prefix= DESTDIR=. install
  ''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

linuxheaders_dep = declare_dependency(
  include_directories : [
    include_directories('include'),
  ],
)

# needs override as its external project
meson.override_dependency('linuxheaders', linuxheaders_dep)
