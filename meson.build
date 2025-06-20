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

gnu_source_check = '''
#include <sys/mman.h>

int
test(void)
{
  return getpagesize() + MAP_ANON;
}
'''

render_width = get_option('minimum_render_width').to_string()

c_args = [    '-Wextra',
    '-g',
    '-Wno-unused-parameter',
    '-Wno-cast-function-type',
    '-fno-omit-frame-pointer',
    '-ffunction-sections',
    '-DN00B_MIN_RENDER_WIDTH=' + render_width,
    '-DN00B_BACKTRACE_SUPPORTED',
    '-std=c2x',    
]

if cc.compiles(glibc_check)
    using_glibc = true
else
    using_glibc = false
endif

if not cc.compiles(gnu_source_check)
  c_args = c_args + ['-D_GNU_SOURCE']
endif  


ncpp_exe = executable(
'ncpp',
  [
    'src/ncpp/ncpp.c',
    'src/ncpp/utils.c',
    'src/ncpp/tokenize.c',
    'src/ncpp/output.c',
    'src/ncpp/xforms.c',
    'src/ncpp/x_keyword.c',
    'src/ncpp/x_once.c',    
  ],
  c_args: [     '-Wextra',
    '-g',
    '-Wno-unused-parameter',
    '-Wno-cast-function-type',
    '-fno-omit-frame-pointer',
    '-ffunction-sections'] + c_args,
    include_directories: incdir)    

ncpp = generator(ncpp_exe, output : '@BASENAME@.c',
                arguments : ['@INPUT@', '@OUTPUT@'])


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
    c_args = c_args + ['-DHAVE_PTY_H']
endif

if get_option('buildtype') == 'release'
    c_args = c_args + ['-Ofast', '-flto']
    link_args = link_args + ['-flto']
endif

if not get_option('use_gc_ptr_hooks').disabled()
    c_args = c_args + ['-DN00B_USE_GC_HOOKS']
endif


if get_option('dev_mode') == true
    c_args = c_args + ['-DN00B_DEV']

    if not get_option('exception_traces').disabled()
        c_args = c_args + ['-DN00B_DEBUG']
    endif

    if get_option('use_asan').enabled()
        c_args = c_args + ['-fsanitize=address', '-fsanitize-recover=all']
        link_args = link_args + ['-fsanitize=address', '-fsanitize-recover=all']

        if (host_machine.system() == 'darwin')
        c_args = c_args + ['-fsanitize-blacklist=../.sanitize_ignore']
        endif
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
            '-DN00B_ALLOC_LOC_INFO',
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
        c_args = c_args + [
             '-DN00B_GC_STATS',
             '-DN00B_ALLOC_LOC_INFO',             
        ]
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

exe_c_args = c_args + ['-flto']
n00b_c_args = c_args



if not using_osx and not using_glibc
    target = build_machine.cpu_family()
    quark_files = ['src/quark/quark.c', 'src/quark' / target + '.s']
else
    quark_files = []
endif

# baked into meson - guaranteed to be present hence not a subproject
libatomic_dep = cc.find_library('atomic', required : false)
libmath_dep = cc.find_library('m', required : false)
libthreads_dep = dependency('threads', required : true)


n00b_core = [
    'src/core/init.nc',
    'src/core/exit.nc',
    'src/core/heap.nc',
    'src/core/heap_alloc.nc',
    'src/core/heap_collect.nc',
    'src/core/kargs.nc',
    'src/core/exceptions.nc',
    'src/core/types.nc',
    'src/core/typestore.nc',
    'src/core/literals.nc',
    'src/core/object.nc',
    'src/core/validation.nc',
    'src/core/signal.nc',
    'src/core/type_hash.nc'      # New type implementation coming soon.
]

n00b_mt = [
    'src/mt/gil.nc',
    'src/mt/lock_accounting.nc',
    'src/mt/mutex.nc',
    'src/mt/condition.nc',    
    'src/mt/rwlock.nc',
    'src/mt/thread.nc',        # Basic thread interface
    'src/mt/thread_tsi.nc',    # Internal API; thread specific data
]

n00b_runtime = [
    'src/runtime/rtsetup.nc',
    'src/runtime/attrstore.nc',
    'src/runtime/vm.nc',
    'src/runtime/ffi.nc',
]

n00b_text = [
    'src/text/string.nc',
    'src/text/style.nc',
    'src/text/colors.nc',
    'src/text/breaks.nc',
    'src/text/rich.nc',
    'src/text/fmt_numbers.nc',
    'src/text/fptostr.nc',
    'src/text/markdown.nc',
    'src/text/theme.nc',
    'src/text/table_utils.nc',
    'src/text/table.nc',
    'src/text/layout.nc',
    'src/text/word_breaks.nc',
    'src/text/ansi.nc',
    'src/text/regex.nc',    
]


n00b_adts = [
    'src/adts/zarray.nc',
    'src/adts/buffer.nc',
    'src/adts/dict.nc',
    'src/adts/set.nc',
    'src/adts/list.nc',
    'src/adts/tree.nc',
    'src/adts/numbers.nc',
    'src/adts/mixed.nc',
    'src/adts/tuple.nc',
    'src/adts/net_addr.nc',
    'src/adts/datetime.nc',
    'src/adts/size.nc',
    'src/adts/duration.nc',
    'src/adts/callback.nc',
    'src/adts/flags.nc',
    'src/adts/box.nc',
    'src/adts/bytering.nc',
]

n00b_io = [
    'src/io/fd_object.nc',
    'src/io/fd_pubsub.nc',
    'src/io/fd_io.nc',
    'src/io/fd_cache.nc',
    'src/io/fd_evloop.nc',
    'src/io/timers.nc',
    'src/io/filter.nc',    
    'src/io/filter_json.nc',
    'src/io/observable.nc',
    'src/io/stream.nc',
    'src/io/stream_fd.nc',
    'src/io/stream_callback.nc',
    'src/io/stream_buffer.nc',
    'src/io/stream_topic.nc',
    'src/io/stream_listener.nc',
    'src/io/stream_exit.nc',
    'src/io/stream_proxy.nc',
    'src/io/stream_string.nc',    
    'src/io/terminal_io.nc',
    'src/io/print.nc',
    'src/io/proc.nc',
    'src/io/session.nc',
    'src/io/session_control.nc',
    'src/io/session_capture.nc',
    'src/io/session_replay.nc',
    'src/io/session_states.nc',
    'src/io/filter_color.nc',
    'src/io/filter_linebuf.nc',
    'src/io/filter_hexdump.nc',
    'src/io/filter_marshal.nc',
    'src/io/filter_ansi.nc',
    'src/io/http.nc',    
]

if get_option('dev_mode') == true
  n00b_io = n00b_io + ['src/io/fd_debug.nc']
endif

n00b_debug = [
    'src/debug/workflow.nc',
    'src/debug/api.nc',
    'src/debug/server.nc',
    'src/debug/memring.nc',
    'src/debug/dlogf.nc',
    'src/debug/streams.nc',    
]

n00b_compiler = [
    'src/compiler/compile.nc',
    'src/compiler/module.nc',
    'src/compiler/lex.nc',
    'src/compiler/parse.nc',
    'src/compiler/errors.nc',
    'src/compiler/scope.nc',
    'src/compiler/specs.nc',
    'src/compiler/cfg_build.nc',
    'src/compiler/cfg.nc',
    'src/compiler/ast_utils.nc',
    'src/compiler/decl_pass.nc',
    'src/compiler/check_pass.nc',
    'src/compiler/memory_layout.nc',
    'src/compiler/codegen.nc',
    'src/compiler/disasm.nc',
]

n00b_util = [
    'src/util/hex.nc',
    'src/util/tree_pattern.nc',
    'src/util/path.nc',
    'src/util/watch.nc',
    'src/util/wrappers.nc',
    'src/util/ctrace.nc',
    'src/util/static_config.nc',
    'src/util/segv.nc',
    'src/util/grammar.nc',
    'src/util/parsing.nc',
    'src/util/parse_tree.nc',
    'src/util/parse_repr.nc',
    'src/util/getopt.nc',
    'src/util/getopt_tokenize.nc',
    'src/util/getopt_parse.nc',
    'src/util/getopt_docs.nc',
    'src/util/json.nc',
    'src/util/assert.nc',
    'src/util/memcheck.nc',
    'src/util/deep_copy.nc',
    'src/util/testgen.nc',
    'src/util/limits.nc',
]

n00b_crypto = ['src/crypto/sha.nc']

n00b_cmd = [
    'src/cmd/main.nc',
    'src/cmd/setup.nc',
    'src/cmd/compile.nc',
    'src/cmd/run.nc',
    'src/cmd/record.nc',
    'src/cmd/play.nc',    
    'src/cmd/test.nc',                        
    'src/cmd/debug.nc',
]


n00b_base = n00b_core + n00b_mt + n00b_text + n00b_runtime + n00b_adts 
n00b_base = n00b_base + n00b_compiler + n00b_util + n00b_crypto
n00b_base = n00b_base + n00b_io + n00b_debug
n00b_src  = ncpp.process(n00b_base)

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
# libcurl depends on openssl so override its dependency first
libcurl_dep = subproject('libcurl').get_variable('libcurl_dep')

if get_option('show_preprocessor_config').enabled()
    c_args = c_args + ['-DN00B_SHOW_PREPROC_CONFIG']
endif

deps = [
  libbacktrace_dep,
  libcurl_dep,
  libffi_dep,
  libpcre_dep,
  libmath_dep,
  libthreads_dep,
  libunibreak_dep,
  libutf8proc_dep,
  linuxheaders_dep,
  openssl_dep,
]

link_with = []

if using_glibc and libatomic_dep.found()
  deps = deps + [libatomic_dep]
endif

n00b = static_library('n00b',
    n00b_src,
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

# executable(
#    'n00btest',
#    test_src,
#    include_directories: incdir,
#    dependencies: deps,
#    c_args: c_args,
#    link_args: exe_link_args,
#    link_with: link_with,
#)

executable(
   'io_demo',
   ['src/c-demos/iodemo.c'] + quark_files,
    include_directories: incdir,
    dependencies: deps,
    c_args: c_args,
    link_args: exe_link_args,
    link_with: link_with,
)   
   
executable(
   'n00b',
   ncpp.process(n00b_cmd + quark_files),
   include_directories: incdir,
   dependencies: deps,
   c_args: c_args,
   link_args: exe_link_args,
   link_with: link_with,
   )


executable(
    'ctest',
    ['tests/c-tests/__cur.c'] + quark_files,
    include_directories: incdir,
    dependencies: deps,
    c_args: c_args,
    link_args: exe_link_args,
    link_with: link_with,
)

executable(
'n00blog',
     ncpp.process(['src/debug/server.nc'] + quark_files),
     include_directories: incdir,
     dependencies: deps,
     c_args: c_args  + ['-DN00B_DEBUG_SERVER',
                      '-Dn00b_debug_entry=main'],
     link_args: exe_link_args,
     link_with: link_with,
)

