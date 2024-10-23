project(
  'openssl',
  'c',
  version : '3.0.8',
  meson_version : '>=1.5.0')

module = import('unstable-external_project')
project = module.add_project(
    'config',
    verbose : true,
    configure_options : [
      # TODO figure out how to match flags in libcurl when skipping modules
      'enable-ec_nistp_64_gcc_128',
      # 'no-afalgeng',
      'no-apps',
      # 'no-bf',
      # 'no-camellia',
      # 'no-cast',
      # 'no-comp',
      # 'no-deprecated',
      # 'no-des',
      'no-docs',
      # 'no-dtls',
      # 'no-dtls1',
      # 'no-egd',
      # 'no-engine',
      # 'no-err',
      # 'no-idea',
      # 'no-md2',
      # 'no-md4',
      # 'no-mdc2',
      # 'no-psk',
      # 'no-quic',
      # 'no-rc2',
      # 'no-rc4',
      # 'no-rc5',
      # 'no-seed',
      # 'no-shared',
      # 'no-srp',
      # 'no-ssl',
      'no-tests',
      # 'no-tls1',
      # 'no-tls1_1',
      # 'no-uplink',
      # 'no-weak-ssl-ciphers',
      # 'no-zlib',
    ])

libcrypto_dep = project.dependency('crypto')
libssl_dep = project.dependency('ssl')
# needs override as its external project
meson.override_dependency('crypto', libcrypto_dep)
meson.override_dependency('ssl', libssl_dep)
