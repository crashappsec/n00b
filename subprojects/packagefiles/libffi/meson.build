project(
  'libffi',
  'c',
  version : '3.4.6',
    default_options: ['c_std=c2x',
                      'prefer_static=true'],
  meson_version : '>=1.5.0')

module = import('unstable-external_project')
project = module.add_project(
    'configure',
    verbose : true,
    configure_options : ['--disable-docs', ])
libffi_dep = project.dependency('ffi')
meson.override_dependency('ffi', libffi_dep)
