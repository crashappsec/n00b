project(
    'n00b',
    'c',
    default_options: [
      'c_std=c2x',
      'default_library=static',
    ],
    version: '0.1.0',
    license: 'MIT',
)

cc = meson.get_compiler('c')
incdir = [include_directories('include')]
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
    '-ffunction-sections',
    '-DHATRACK_PER_INSTANCE_AUX',
    '-DHATRACK_DONT_DEALLOC',
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
    link_args = ['-static']
    c_args = c_args + ['-D_GNU_SOURCE']

    # This doesn't seem to always work right on Linux so don't use it by default.
    if get_option('use_frame_intrinsic').enabled()
        c_args = c_args + ['-DN00B_USE_FRAME_INTRINSIC']
    endif
endif

if using_osx
    dead_strip = '-dead_strip'
    #    do_dead_strip = true
    do_dead_strip = false
else
    dead_strip = '--gc-sections'
    do_dead_strip = false
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
        c_args = c_args + ['-DN00B_GC_STATS',
                '-DHATRACK_ALLOC_PASS_LOCATION']
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

if do_dead_strip
    exe_link_args = exe_link_args + [dead_strip]
endif

if using_osx
    exe_link_args = exe_link_args + ['-Wl']
endif

exe_c_args = c_args + ['-flto', '-DHATRACK_REFERENCE_ALGORITHMS']
n00b_c_args = c_args


n00b_core = [
    'src/core/init.c',
    'src/core/exit.c',
    'src/core/heap.c',
    'src/core/heap_alloc.c',
    'src/core/heap_collect.c',
    'src/core/kargs.c',
    'src/core/exceptions.c',
    'src/core/types.c',
    'src/core/typestore.c',
    'src/core/literals.c',
    'src/core/object.c',
    'src/core/validation.c',
    'src/core/thread.c',        # Basic thread interface
    'src/core/thread_tsi.c',    # Internal API; thread specific data
    'src/core/thread_coop.c',   # Mainly internal API for stop-the-world support
    'src/core/lock.c',     
]

n00b_runtime = [
    'src/runtime/rtsetup.c',
    'src/runtime/attrstore.c',
    'src/runtime/vm.c',
    'src/runtime/ffi.c',
]

n00b_text = [
    'src/text/string.c',
    'src/text/style.c',
    'src/text/colors.c',
    'src/text/breaks.c',
    'src/text/rich.c',
    'src/text/fmt_numbers.c',
    'src/text/fptostr.c',
    'src/text/markdown.c',
    'src/text/theme.c',
    'src/text/table_utils.c',
    'src/text/table.c',
    'src/text/layout.c',
    'src/text/word_breaks.c',
    'src/text/ansi.c',
    'src/text/regex.c',    
]


n00b_adts = [
    'src/adts/buffer.c',
    'src/adts/dict.c',
    'src/adts/set.c',
    'src/adts/hatlists.c',
    'src/adts/list.c',
    'src/adts/tree.c',
    'src/adts/numbers.c',
    'src/adts/mixed.c',
    'src/adts/tuple.c',
    'src/adts/net_addr.c',
    'src/adts/datetime.c',
    'src/adts/size.c',
    'src/adts/duration.c',
    'src/adts/callback.c',
    'src/adts/flags.c',
    'src/adts/box.c',
    'src/adts/bytering.c',
]

n00b_io = [
    'src/io/iocore.c',
    'src/io/ioqueue.c',
    'src/io/filters.c',
    'src/io/marshal.c',
    'src/io/file.c',
    'src/io/socket.c',
    'src/io/listener.c',
    'src/io/signal.c',
    'src/io/timer.c',
    'src/io/exitinfo.c',
    'src/io/condition.c',
    'src/io/callback.c',
    'src/io/topic.c',
    'src/io/subproxy.c',
    'src/io/buffer.c',
    'src/io/strstream.c',
    'src/io/colorterm.c',
    'src/io/print.c',
    'src/io/term.c',
    'src/io/http.c',
    'src/io/proc.c',
    'src/io/debug.c',
    'src/io/session.c',
    'src/io/session_control.c',
    'src/io/session_capture.c',
    'src/io/session_replay.c',
    'src/io/session_states.c',
]

n00b_channels = [
    'src/channels/fd_events.c',
    'src/channels/observable.c',
    'src/channels/channel.c',
    'src/channels/chan_fd.c',
    'src/channels/chan_callback.c',
    'src/channels/chan_buffer.c',
    'src/channels/chan_topic.c',
    'src/channels/chan_listener.c',        
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
    'src/util/hex.c',
    'src/util/tree_pattern.c',
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
    'src/util/json.c',
    'src/util/assert.c',
    'src/util/memcheck.c',
    'src/util/deep_copy.c',
    'src/util/testgen.c',
    'src/util/notify.c',
]

n00b_crypto = ['src/crypto/sha.c']

n00b_cmd = [
    'src/cmd/main.c',
    'src/cmd/setup.c',
    'src/cmd/compile.c',
    'src/cmd/run.c',
    'src/cmd/record.c',
    'src/cmd/play.c',    
    'src/cmd/test.c',                        
    'src/cmd/debug.c',
]


n00b_log = ['src/cmd/n00b_debug.c']

n00b_base = n00b_core + n00b_text + n00b_runtime + n00b_adts + n00b_io 
n00b_base = n00b_base + n00b_compiler + n00b_util + n00b_crypto
n00b_base = n00b_base + n00b_channels

n00b_src = n00b_base + n00b_cmd
log_src = n00b_base + n00b_log

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

lib_src = n00b_base + hat_primary

test_src = [
    'src/harness/n00b_base/test.c',
    'src/harness/n00b_base/scan.c',
    'src/harness/n00b_base/run.c',
    'src/harness/n00b_base/validation.c',
    'src/harness/n00b_base/results.c',
]

# baked into meson - guaranteed to be present hence not a subproject
libatomic_dep = cc.find_library('atomic', required : false)
libmath_dep = cc.find_library('m', required : false)
libthreads_dep = dependency('threads', required : true)

# https://stackoverflow.com/questions/73053163/how-to-force-meson-to-use-only-wrap-subproject
if (host_machine.system() == 'darwin')
  linuxheaders_dep = declare_dependency()
else
  linuxheaders_dep = subproject('linuxheaders').get_variable('linuxheaders_dep')
endif
libbacktrace_dep = subproject('libbacktrace').get_variable('libbacktrace_dep')
libffi_dep = subproject('libffi').get_variable('libffi_dep')
libpcre_dep = subproject('pcre2').get_variable('libpcre2_8')
libunibreak_dep = subproject('libunibreak').get_variable('libunibreak_dep')
libutf8proc_dep = subproject('libutf8proc').get_variable('utf8proc_dep')
openssl_dep = subproject('openssl').get_variable('openssl_dep')
# libcurl and libevent depend on openssl so override its dependency first
libcurl_dep = subproject('libcurl').get_variable('libcurl_dep')
libevent_dep = subproject('libevent').get_variable('libevent_dep')

if get_option('show_preprocessor_config').enabled()
    c_args = c_args + ['-DN00B_SHOW_PREPROC_CONFIG']
endif

deps = [
  libbacktrace_dep,
  libcurl_dep,
  libevent_dep,
  libffi_dep,
  libpcre_dep,
  libmath_dep,
  libthreads_dep,
  libunibreak_dep,
  libutf8proc_dep,
  linuxheaders_dep,
  openssl_dep,
]
link_with = [
]

if using_glibc and libatomic_dep.found()
  deps = deps + [libatomic_dep]
endif

n00b = static_library('n00b',
    lib_src,
    include_directories: incdir,
    dependencies: deps,
    c_args: n00b_c_args,
    link_args: link_args,
    link_with: link_with,
)

link_with = link_with + [n00b]

if get_option('build_n00b_dll').enabled()
    library(
        'n00b-dll',
        lib_src,
        include_directories: incdir,
        dependencies: deps,
        c_args: n00b_c_args,
        link_args: link_args,
    )
endif

executable(
    'n00btest',
    test_src,
    include_directories: incdir,
    dependencies: deps,
    c_args: c_args,
    link_args: exe_link_args,
    link_with: link_with,
)

executable(
    'n00b',
    n00b_src,    
    include_directories: incdir,
    dependencies: deps,
    c_args: c_args,
    link_args: exe_link_args,
    link_with: link_with,
)

executable(
    'ctest',
    ['tests/c-tests/__cur.c'],
    include_directories: incdir,
    dependencies: deps,
    c_args: c_args,
    link_args: exe_link_args,
    link_with: link_with,
)

executable(
 'n00blog',
 log_src,
    include_directories: incdir,
    dependencies: deps,
    c_args: c_args,
    link_args: exe_link_args,
    link_with: link_with,
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
        dependencies: deps,
        link_args: exe_link_args,
        link_with: [libhat],
        c_args: exe_c_args,
    )
endif
