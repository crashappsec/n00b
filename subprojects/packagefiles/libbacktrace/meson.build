project(
  'libbacktrace',
  'c',
  version : '1.0',
  default_options: [
    'c_std=c2x',
   'prefer_static=true',
  ],
  meson_version : '>=1.5.0')

i = run_command(
  'sh', '-exc', '''
[ -n "$(find . -name libbacktrace.a)" ] && exit 0
./configure
make
''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

libbacktrace_dep = declare_dependency(
  include_directories : [
    include_directories('.'),
  ],
  link_args : [
    '-L' + meson.current_source_dir() + '/.libs',
    '-lbacktrace',
  ],
)

# needs override as its external project
meson.override_dependency('libbacktrace', libbacktrace_dep)
