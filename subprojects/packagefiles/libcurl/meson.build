# not using wrapdb version as it doesnt find lfind
# and is generally pretty complicated meson.build script
# whereas configure+make work just fine
project(
  'curl',
  'c',
  version: '8.9.1',
  meson_version : '>=1.5.0')

module = import('unstable-external_project')
project = module.add_project(
    'configure',
    verbose : true,
    configure_options : ['--with-openssl', ])

libcurl_dep = project.dependency('curl')
# needs override as its external project
meson.override_dependency('curl', libcurl_dep)
