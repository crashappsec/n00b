project(
  'libffi',
  'c',
  version : '3.4.6',
  default_options: [
    'c_std=c2x',
    'prefer_static=true',
  ],
  meson_version : '>=1.5.0',
)

if (host_machine.system() == 'darwin')
  linuxheaders_dep = declare_dependency()
else
  linuxheaders_dep = dependency('linuxheaders')
endif

i = run_command(
  'sh',
  '-exc',
  '''
[ -n "$(find . -name libffi.a)" ] && exit 0
if [ "$(uname)" != "Darwin" ] ; then
export CPPFLAGS="-I$(realpath ../kernel-headers-*)/include $CPPFLAGS"
fi
./configure \
  --disable-docs
make
  ''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

# ffi creates .libs in a subfolder
# so find it dynamically
libs = run_command(
  'sh',
  '-exc',
  '''
dirname $(find $PWD -name libffi.a)
  ''',
  capture : true,
  check : true,
).stdout().splitlines()[-1]
message(libs)

libffi_dep = declare_dependency(
  dependencies : [
    linuxheaders_dep,
  ],
  include_directories : [
    include_directories('.'),
  ],
  link_args : [
    '-L' + libs,
    '-lffi',
  ],
)

# needs override as its external project
meson.override_dependency('libffi', libffi_dep)
