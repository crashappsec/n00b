# not using wrapdb version as it doesnt find lfind
# and is generally pretty complicated meson.build script
# whereas configure+make work just fine
project(
  'curl',
  'c',
  version: '8.9.1',
  meson_version : '>=1.5.0',
)

# module = import('unstable-external_project')
# project = module.add_project(
#     'configure',
#     verbose : true,
#     configure_options : [
#       '--with-openssl',
#     ],
# )
# libcurl = project.dependency('curl')

libcrypto_dep = subproject('openssl').get_variable('libcrypto_dep')
libssl_dep = subproject('openssl').get_variable('libssl_dep')

libcurl = library(
  'curl',
  dependencies : [
    libcrypto_dep,
    libssl_dep,
  ],
)

libcurl_dep = declare_dependency(
  link_with : [libcurl],
  include_directories : [include_directories('include')],
)
meson.override_dependency('libcurl', libcurl_dep)
