# wrapdb doesnt define a dep variable to reference
# in parent meson build
project(
  'libunibreak',
  'c',
  version: '5.1',
  meson_version: '>=0.59.0',
)

module = import('unstable-external_project')
project = module.add_project(
    'configure',
    verbose : true,)

libunibreak_dep = project.dependency('unibreak')
# needs override as its external project
meson.override_dependency('libunibreak', libunibreak_dep)
