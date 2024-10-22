project(
    'n00b',
    'c',
    default_options: ['c_std=c2x',
                      'default_library=static'],
    version: '0.1.0',
    license: 'MIT',
)

cc = meson.get_compiler('c')
incdir = include_directories('include')
using_osx = false

glibc_check = '''
#include <stdio.h>
#ifndef __GLIBC__
#error NO GLIBC
#endif
'''
if cc.compiles(glibc_check)
    using_glibc = true
else
    using_glibc = false
endif

render_width = get_option('minimum_render_width').to_string()

c_args = [
    '-Wextra',
    '-g',
    '-Wno-unused-parameter',
    '-Wno-cast-function-type',
    '-fno-omit-frame-pointer',
    '-DHATRACK_PER_INSTANCE_AUX',
    '-DN00B_MIN_RENDER_WIDTH=' + render_width,
    '-DN00B_BACKTRACE_SUPPORTED',
]

if (host_machine.cpu_family() == 'x86_64' and cc.get_id() == 'clang')
    c_args = c_args + ['-Wno-atomic-alignment']
endif

if (host_machine.system() == 'darwin')
    using_osx = true
    link_args = ['-target', 'arm64-apple-macos14', '-framework', 'Security']

    if not get_option('use_frame_intrinsic').disabled()
        c_args = c_args + ['-DN00B_USE_FRAME_INTRINSIC']
    endif

else
    using_osx = false
    link_args = []
    c_args = c_args + ['-D_GNU_SOURCE']

    # This doesn't seem to always work right on Linux so don't use it by default.
    if get_option('use_frame_intrinsic').enabled()
        c_args = c_args + ['-DN00B_USE_FRAME_INTRINSIC']
    endif
endif

if get_option('forkless_tests').enabled()
    c_args = c_args + ['-DN00B_TEST_WITHOUT_FORK']
endif

if not get_option('static_ffi_binding').disabled()
  c_args = c_args + ['-DN00B_STATIC_FFI_BINDING']
endif

fpty_code = '''
#include <stddef.h>
#include <pty.h>

int main(void) { forkpty(NULL, NULL, NULL, NULL); return 0; }
'''

if cc.links(fpty_code, name: 'forkpty_check')
    add_project_arguments('-DHAVE_PTY_H', language: 'c')
endif

if get_option('buildtype') == 'release'
    c_args = c_args + ['-Ofast', '-flto']
    link_args = link_args + ['-flto']
endif

if not get_option('use_gc_ptr_hooks').disabled()
    c_args = c_args + ['-DN00B_USE_GC_HOOKS']
endif

if get_option('keep_alloc_locations') == true and get_option('dev_mode') == false
    c_args = c_args + ['-DHATRACK_ALLOC_PASS_LOCATION']

endif

if get_option('dev_mode') == true
    c_args = c_args + ['-DN00B_DEV']

    if not get_option('exception_traces').disabled()
        c_args = c_args + ['-DN00B_DEBUG']
    endif

    if get_option('use_asan').enabled()
        c_args = c_args + ['-fsanitize=address', '-fsanitize-recover=all']
        link_args = link_args + ['-fsanitize=address', '-fsanitize-recover=all']
    endif

    if get_option('use_ubsan').enabled()
        c_args = c_args + [
            '-fsanitize=undefined',
            '-fno-sanitize=function',
            '-fsanitize-recover=all',
        ]
        link_args = link_args + [
            '-fsanitize=undefined',
            '-fsanitize-recover=all',
        ]
    endif

    memcheck = get_option('use_memcheck')
    if not (memcheck == 'off')
        c_args = c_args + [
            '-DN00B_FULL_MEMCHECK',
            '-DHATRACK_ALLOC_PASS_LOCATION',
        ]

        if memcheck == 'strict'
            c_args = c_args + ['-DN00B_STRICT_MEMCHECK']
        endif

        show_count = get_option('memcheck_show_allocs')
        if show_count != 0
            as_str = show_count.to_string()
            c_args = c_args + ['-DN00B_SHOW_NEXT_ALLOCS=' + as_str]
        endif
    endif

    if get_option('show_gc_stats').enabled()
        c_args = c_args + ['-DN00B_GC_STATS', '-DHATRACK_ALLOC_PASS_LOCATION']
    endif

    vm_debug = get_option('vm_debug')

    if vm_debug != 'always off'
        c_args = c_args + ['-DN00B_VM_DEBUG']
    endif

    if vm_debug == 'default on'
        c_args = c_args + ['-DN00B_VM_DEBUG_DEFAULT=true']
    endif

    if get_option('warn_on_zero_allocs') == true
        c_args = c_args + ['-DN00B_WARN_ON_ZERO_ALLOCS']
    endif


    gctrace = get_option('gc_tracing')

    if gctrace != 'off'
        c_args = c_args + ['-DN00B_GC_FULL_TRACE']
    endif

    if gctrace == 'full'
        c_args = c_args + ['-DN00B_GC_FULL_TRACE', '-DN00B_GC_ALL_ON']
    endif

endif

exe_link_args = link_args + ['-flto', '-w']
exe_c_args = c_args + ['-flto', '-DHATRACK_REFERENCE_ALGORITHMS']

n00b_c_args = c_args


n00b_core = [
    'src/core/init.c',
    'src/core/gcbase.c',
    'src/core/collect.c',
    'src/core/kargs.c',
    'src/core/exceptions.c',
    'src/core/types.c',
    'src/core/typestore.c',
    'src/core/marshal.c',
    'src/core/unmarshal.c',
    'src/core/literals.c',
    'src/core/attrstore.c',
    'src/core/vm.c',
    'src/core/ffi.c',
    'src/core/object.c',
    'src/core/validation.c',
    'src/core/rtsetup.c',
]
n00b_adts = [
    'src/adts/string.c',
    'src/adts/buffer.c',
    'src/adts/dict.c',
    'src/adts/set.c',
    'src/adts/hatlists.c',
    'src/adts/list.c',
    'src/adts/grid.c',
    'src/adts/tree.c',
    'src/adts/numbers.c',
    'src/adts/mixed.c',
    'src/adts/tuple.c',
    'src/adts/ipaddr.c',
    'src/adts/datetime.c',
    'src/adts/size.c',
    'src/adts/duration.c',
    'src/adts/callback.c',
    'src/adts/streams.c',
    'src/adts/flags.c',
    'src/adts/box.c',
]

n00b_io = [
    'src/io/ansi.c',
    'src/io/switchboard.c',
    'src/io/subproc.c',
    'src/io/term.c',
    'src/io/http.c',
    'src/io/file.c',
]

n00b_compiler = [
    'src/compiler/compile.c',
    'src/compiler/module.c',
    'src/compiler/lex.c',
    'src/compiler/parse.c',
    'src/compiler/errors.c',
    'src/compiler/scope.c',
    'src/compiler/specs.c',
    'src/compiler/cfg_build.c',
    'src/compiler/cfg.c',
    'src/compiler/ast_utils.c',
    'src/compiler/decl_pass.c',
    'src/compiler/check_pass.c',
    'src/compiler/memory_layout.c',
    'src/compiler/codegen.c',
    'src/compiler/disasm.c',
]

n00b_util = [
    'src/util/style.c',
    'src/util/styledb.c',
    'src/util/colors.c',
    'src/util/breaks.c',
    'src/util/hex.c',
    'src/util/tree_pattern.c',
    'src/util/conststr.c',
    'src/util/richlit.c',
    'src/util/format.c',
    'src/util/fptostr.c',
    'src/util/path.c',
    'src/util/watch.c',
    'src/util/wrappers.c',
    'src/util/ctrace.c',
    'src/util/static_config.c',
    'src/util/segv.c',
    'src/util/grammar.c',
    'src/util/parsing.c',
    'src/util/parse_tree.c',
    'src/util/parse_repr.c',
    'src/util/getopt.c',
    'src/util/getopt_tokenize.c',
    'src/util/getopt_parse.c',
    'src/util/getopt_docs.c',
    'src/util/markdown.c',
]

n00b_crypto = ['src/crypto/sha.c']

n00b_src = n00b_core + n00b_adts + n00b_io + n00b_compiler + n00b_util + n00b_crypto


hat_primary = [
    'src/hatrack/support/hatrack_common.c',
    'src/hatrack/support/mmm.c',
    'src/hatrack/support/counters.c',
    'src/hatrack/support/malloc.c',
    'src/hatrack/hash/crown.c',
    'src/hatrack/hash/dict.c',
    'src/hatrack/hash/xxhash.c',
    'src/hatrack/hash/set.c',
    'src/hatrack/hash/woolhat.c',
    'src/hatrack/array/flexarray.c',
    'src/hatrack/array/zeroarray.c',
    'src/hatrack/queue/queue.c',
    'src/hatrack/queue/hatring.c',
    'src/hatrack/queue/stack.c',
    'src/hatrack/queue/debug.c',
    'src/hatrack/queue/logring.c',
]

hat_hashref = [
    'src/hatrack/hash/ballcap.c',
    'src/hatrack/hash/duncecap.c',
    'src/hatrack/hash/hihat-a.c',
    'src/hatrack/hash/hihat.c',
    'src/hatrack/hash/lohat-a.c',
    'src/hatrack/hash/lohat.c',
    'src/hatrack/hash/newshat.c',
    'src/hatrack/hash/oldhat.c',
    'src/hatrack/hash/refhat.c',
    'src/hatrack/hash/swimcap.c',
    'src/hatrack/hash/tiara.c',
    'src/hatrack/hash/tophat.c',
    'src/hatrack/hash/witchhat.c',
]

hash_test_src = hat_hashref + [
    'src/harness/hash/test.c',
    'src/harness/hash/default.c',
    'src/harness/hash/performance.c',
    'src/harness/hash/config.c',
    'src/harness/hash/functional.c',
    'src/harness/hash/rand.c',
    'src/harness/hash/testhat.c',
]

if not using_osx and not using_glibc
    target = build_machine.cpu_family()
    quark_files = ['src/quark/quark.c', 'src/quark' / target + '.s']
    hat_primary = hat_primary + quark_files
endif

lib_src = n00b_src + hat_primary

test_src = [
    'src/harness/n00b_base/test.c',
    'src/harness/n00b_base/scan.c',
    'src/harness/n00b_base/run.c',
    'src/harness/n00b_base/validation.c',
    'src/harness/n00b_base/results.c',
]

threads = dependency('threads')

math = cc.find_library('m', required: false)


unibreak = dependency('libunibreak')
utf8proc = dependency('libutf8proc')

libcurl = cc.find_library('curl', required: false)
if not libcurl.found()
  libcurl  = dependency('libcurl', fallback: ['curl'])
endif

crypto = cc.find_library('crypto')
ssl = cc.find_library('ssl')

if not crypto.found()
    openssl  = dependency('openssl', fallback: ['openssl'])
endif

ffi = cc.find_library('ffi')
if not ffi.found()
    ffi      = dependency('libffi',
                          fallback: ['libffi', 'libffi_dep'])
endif

backtrace = cc.find_library('backtrace')
if not backtrace.found()
    backtrace = dependency('backtrace',
                       fallback: ['libbacktrace', 'libbacktrace_dep'])
endif

#zlib = cc.find_library('z')

#if not zlib.found()
#    zlib      = dependency('zlib')
#endif


if get_option('show_preprocessor_config').enabled()
    c_args = c_args + ['-DN00B_SHOW_PREPROC_CONFIG']
endif

deps = [unibreak, utf8proc, threads, ffi, backtrace, libcurl, ssl]
opts = [math, crypto]

if using_glibc
    opts = opts + [cc.find_library('atomic')]
endif
all_deps = deps + opts

n00b = static_library(
    'n00b',
    lib_src,
    include_directories: incdir,
    dependencies: all_deps,
    c_args: n00b_c_args,
    link_args: link_args,
)


if get_option('build_n00b_dll').enabled()
    library(
        'n00b-dll',
        lib_src,
        include_directories: incdir,
        dependencies: all_deps,
        c_args: n00b_c_args,
        link_args: link_args,
    )
endif

executable(
    'n00btest',
    test_src,
    include_directories: incdir,
    dependencies: [all_deps],
    c_args: c_args,
    link_args: exe_link_args,
    link_with: n00b,
)

if get_option('build_hatrack').enabled()
    libhat = static_library(
        'hatrack',
        lib_src,
        include_directories: incdir,
        c_args: c_args,
        link_args: link_args,
    )

    executable(
        'hash',
        hash_test_src,
        include_directories: incdir,
        dependencies: all_deps,
        link_args: exe_link_args,
        link_with: libhat,
        c_args: exe_c_args,
    )
endif
