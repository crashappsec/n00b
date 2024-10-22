project(
  'libbacktrace',
  'c',
  version : '1.0',
  default_options: ['c_std=c2x',
                    'prefer_static=true'],
  meson_version : '>=1.5.0')

module = import('unstable-external_project')
project = module.add_project(
    'configure',
    verbose : true,)

libbacktrace_dep = project.dependency('backtrace')
meson.override_dependency('backtrace', libbacktrace_dep)