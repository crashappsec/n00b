# not using wrapdb version as it doesnt find lfind
# and is generally pretty complicated meson.build script
# whereas configure+make work just fine
project(
  'libcurl',
  'c',
  version: '8.9.1',
  meson_version : '>=1.5.0',
)

openssl_dep = dependency('openssl')

i = run_command(
  'sh',
  '-exc',
  '''
[ -n "$(find -name libcurl.a)" ] && exit 0
export CPPFLAGS="-I$(realpath ../openssl-*)/include $CPPFLAGS"
export LDFLAGS="-L$(realpath ../openssl-*) $LDFLAGS"
if [ "$(uname)" != "Darwin" ] ; then
export LDFLAGS="-static $LDFLAGS"
fi
./configure -v \
  --with-openssl=$(realpath ../openssl-*) \
  --disable-docs \
  --disable-ntlm \
  --without-brotli \
  --without-libgsasl \
  --without-zlib \
  --without-zstd
make
  ''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

libcurl_dep = declare_dependency(
  dependencies : [
    openssl_dep,
  ],
  include_directories : [
    include_directories('./include'),
  ],
  link_args : [
    '-L' + meson.current_source_dir() + '/lib/.libs',
    '-lcurl',
  ],
)

meson.override_dependency('libcurl', libcurl_dep)
