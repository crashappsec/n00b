project(
  'libevent',
  'c',
  version : '2.1.12',
  meson_version : '>=1.5.0',
)

openssl_dep = dependency('openssl')

i = run_command(
  'sh',
  '-exc',
  '''
[ -n "$(find . -name libevent.a)" ] && exit 0
export CPPFLAGS="-I$(realpath ../openssl-*)/include -DNDEBUG $CPPFLAGS"
export LDFLAGS="-L$(realpath ../openssl-*) $LDFLAGS"
if [ "$(uname)" != "Darwin" ] ; then
export LDFLAGS="-static $LDFLAGS"
fi
./configure
make
  ''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

libevent_dep = declare_dependency(
  dependencies : [
    openssl_dep,
  ],
  include_directories : [
    include_directories('./include'),
  ],
  link_args : [
    '-L' + meson.current_source_dir() + '/.libs',
    '-levent_core',
    '-levent_extra',
    '-levent_pthreads',
    '-levent_openssl',
  ]
)

meson.override_dependency('libevent', libevent_dep)
