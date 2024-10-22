project(
  'openssl',
  'c',
  version : '3.0.8',
  meson_version : '>=1.5.0',
)

i = run_command(
  'sh',
  '-exc',
  '''
[ -n "$(find -name libcrypto.a)" ] && exit 0
# cannot disable deprecated/engine as curl uses these
# -no-deprecated
# -no-engine
if [ "$(uname)" != "Darwin" ] ; then
export N00B_STATIC=-static
fi

./config \
  enable-ec_nistp_64_gcc_128 \
  no-afalgeng \
  no-apps \
  no-bf \
  no-camellia \
  no-cast \
  no-comp \
  no-des \
  no-docs \
  no-dtls \
  no-dtls1 \
  no-egd \
  no-err \
  no-idea \
  no-md2 \
  no-md4 \
  no-mdc2 \
  no-psk \
  no-quic \
  no-rc2 \
  no-rc4 \
  no-rc5 \
  no-seed \
  no-shared \
  no-srp \
  no-ssl \
  no-tests \
  no-tls1 \
  no-tls1_1 \
  no-uplink \
  no-weak-ssl-ciphers \
  no-zlib \
  ${N00B_STATIC}
make build_libs
  ''',
  capture : true,
  check : true,
)
message(i.stdout())
message(i.stderr())

openssl_dep = declare_dependency(
  include_directories : [
    include_directories('./include/openssl'),
  ],
  link_args : [
    '-L' + meson.current_source_dir(),
    '-lcrypto',
    '-lssl',
  ],
)

meson.override_dependency('openssl', openssl_dep)
