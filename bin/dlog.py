import sys, os

# This script is meant to be run when we add a new internal debug topic to
# automatically generate dlogf.h
#
# The current default behavior compiles in code to log l0 and l1
# messages, but does not log l1 messages by default.

dlog_topics = ["lock", "alloc", "gc", "gil", "thread", "io"]

# If this is false, all dlog stuff is disabled at compile time
# by default.
dlog_level_to_compile_out = 2
#
# Log levels this level and higher are DISABLED by default, but
# available at runtime.
dlog_run_time_first_default_disabled_level = 2

# Specific default log levels per topic (anything below the number
# gets recorded, without having to be runtime enabled. Only sets the
# default if they are below the 'compiled out' threshold.

dlog_topic_defaults = {}

# Don't change this.
dlogf_header = "../include/debug/dlogf.h"

def dlog_macro_name(topic, level):
    result = "n00b_dlog_" + topic
    if level > 0:
        result += str(level)
    return result

def gen_dlog_enabled_def(topic, level):
    return """#define %s(...)\\
    n00b_dlog("%s", N00B_DLOG_%s_IX, %d, __FILE__, __LINE__, \\
              n00b_dstrf(__VA_ARGS__))
""" % (dlog_macro_name(topic, level), topic, topic.upper(), level)


def gen_dlog_disabled_def(topic, level):
    return '#define ' + dlog_macro_name(topic, level) + '(...)'

assert(dlog_level_to_compile_out >= dlog_run_time_first_default_disabled_level)

def generate_toplevel_defines():
    return """#pragma once

#if defined(N00B_DEBUG)
extern char   *_n00b_dstrf(char *fmt, int64_t num_params, ...);
extern void    n00b_dlog(char *, int, int, char *,  int, char *);
extern int64_t n00b_dlog_get_topic_policy(char *);
extern bool n00b_dlog_set_topic_policy(char *, int64_t);

#else

#undef N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_COMPILE_OUT_THRESHOLD 4
#undef N00B_DLOG_DEFAULT_DISABLE_LEVEL
#define N00B_DLOG_DEFAULT_DISABLE_LEVEL 4

#define n00b_dlog_get_topic_policy(x) 0
#define n00b_dlog_set_topic_policy(a, b) false
#define _n00b_dstrf(...)
#define n00b_dlog(...)

#endif

#define n00b_dstrf(fmt, ...) \
    _n00b_dstrf(fmt, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, __VA_ARGS__))

#if !defined(N00B_DLOG_COMPILE_OUT_THRESHOLD)
#define N00B_DLOG_COMPILE_OUT_THRESHOLD %s
#endif

#if !defined(N00B_DLOG_DEFAULT_DISABLE_LEVEL)
#define N00B_DLOG_DEFAULT_DISABLE_LEVEL %s
#endif

#if N00B_DLOG_DEFAULT_DISABLE_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#warning "N00B_DLOG_COMPILE_OUT_THRESHOLD <= N00B_DLOG_DEFAULT_DISABLE_LEVEL"
#endif
""" % (dlog_level_to_compile_out, dlog_run_time_first_default_disabled_level)

def gen_one_ll_condition(topic, topic_uc, level):
    return """
#if N00B_DLOG_%s_COMPILE_OUT_THRESHOLD <= %d
%s
#else
%s
#endif
""" % (topic_uc,
       level,
       gen_dlog_disabled_def(topic, level),
       gen_dlog_enabled_def(topic, level))

def generate_topic_ll_defines(topic):
    uc = topic.upper()
    default_level = dlog_run_time_first_default_disabled_level
    if topic in dlog_topic_defaults:
        default_level = dlog_topic_defaults[topic]

    return """
#if defined(N00B_DEBUG)
#if !defined(N00B_DLOG_%s_DEFAULT_LAST_LEVEL)
#define N00B_DLOG_%s_DEFAULT_LAST_LEVEL %d
#endif

#if N00B_DLOG_%s_DEFAULT_LAST_LEVEL > N00B_DLOG_COMPILE_OUT_THRESHOLD
#define N00B_DLOG_%s_COMPILE_OUT_THRESHOLD N00B_DLOG_%s_DEFAULT_LAST_LEVEL
#else
#define N00B_DLOG_%s_COMPILE_OUT_THRESHOLD N00B_DLOG_COMPILE_OUT_THRESHOLD
#endif
#else // N00B_DEBUG
#define N00B_DLOG_%s_COMPILE_OUT_THRESHOLD 4 // Don't compile any dlogs
#endif // N00B_DEBUG
%s
%s
%s
%s
#if N00B_DLOG_%s_COMPILE_OUT_THRESHOLD > 0
#define N00B_DLOG_%s_ON
#endif
#define N00B_DLOG_%s_LEVEL (N00B_DLOG_%s_COMPILE_OUT_THRESHOLD - 1)
""" % (uc, uc, default_level, uc, uc, uc, uc, uc,
       gen_one_ll_condition(topic, uc, 3),
       gen_one_ll_condition(topic, uc, 2),
       gen_one_ll_condition(topic, uc, 1),
       gen_one_ll_condition(topic, uc, 0),
       uc, uc, uc, uc)

def one_init(topic):
    return  """    {.topic = "%s", .value =  N00B_DLOG_%s_DEFAULT_LAST_LEVEL - 1ULL},\\
""" % (topic, topic.upper())

def generate_ix_map():
    result = ""

    i = 0
    for item in dlog_topics:
        result += "#define N00B_DLOG_%s_IX %d\n" % (item.upper(), i)
        i += 1

    return result

def generate_inits():
    result = """
#if defined(N00B_DEBUG)
#define N00B_DLOG_GENERATED_INITS\\
    {\\
"""
    for item in dlog_topics:
        result += one_init(item)

    return result + """};

#define N00B_DLOG_NUM_TOPICS %d
#endif
""" % len(dlog_topics)



output = generate_toplevel_defines()
output += "\n"
output += generate_ix_map()
output += "\n"

for item in dlog_topics:
    output += generate_topic_ll_defines(item)
    output += "\n"

output += "\n"
output += generate_inits()

path = os.path.realpath(__file__)
parts = os.path.split(path)
path = os.path.join(parts[0], dlogf_header)
path = os.path.abspath(path)

f = open(path, 'w')
f.write(output)
f.close()

print("Updated internal debug log header @ %s with current topics" % path)
