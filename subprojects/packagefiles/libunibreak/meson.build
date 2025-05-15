# wrapdb doesnt define a dep variable to reference
# in parent meson build
project(
  'libunibreak',
  'c',
  version: '5.1',
  meson_version: '>=0.59.0',
)

i = run_command(
  'sh',
  '-exc',
  '''
[ -n "$(find . -name libunibreak.a)" ] && exit 0
./configure
make
  ''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

libunibreak_dep = declare_dependency(
  include_directories : [
    include_directories('./src'),
  ],
  link_args : [
    '-L' + meson.current_source_dir() + '/src/.libs',
    '-lunibreak',
  ],
)

# needs override as its external project
meson.override_dependency('libunibreak', libunibreak_dep)
