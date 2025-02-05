#define N00B_USE_INTERNAL_API
#include "n00b/test_harness.h"

int            n00b_test_total_items       = 0;
int            n00b_test_total_tests       = 0;
int            n00b_non_tests              = 0;
_Atomic int    n00b_test_number_passed     = 0;
_Atomic int    n00b_test_number_failed     = 0;
_Atomic int    n00b_test_next_test         = 0;
n00b_test_kat *n00b_test_info              = NULL;
bool           n00b_give_malformed_warning = false;
bool           n00b_dev_mode               = false;
int            n00b_current_test_case      = 0;
int            n00b_watch_case             = 5;
size_t         n00b_term_width;

#ifdef N00B_FULL_MEMCHECK
extern bool n00b_definite_memcheck_error;
#else
bool n00b_definite_memcheck_error = false;
#endif

void
add_static_test_symbols()
{
    n00b_add_static_symbols();
    n00b_add_static_function(n00b_cstring("strndup"),
                             strndup);
}

static void
one_parse(n00b_parser_t *parser, char *s)
{
    n00b_string_t *input = n00b_cstring(s);
    n00b_string_t *ctext = n00b_cformat("'«#»'", input);

    n00b_print(n00b_call_out(ctext));

    n00b_parse_string(parser, input, NULL);

    n00b_list_t *l = n00b_parse_get_parses(parser);

    n00b_print(n00b_forest_format(l));
}

void
test_parsing(void)
{
    n00b_grammar_t *grammar = n00b_new(n00b_type_grammar(),
                                       n00b_kw("detect_errors",
                                               n00b_ka(true)));
    n00b_pitem_t   *add     = n00b_pitem_nonterm_raw(grammar,
                                               n00b_cstring("Add"));
    n00b_pitem_t   *mul     = n00b_pitem_nonterm_raw(grammar,
                                               n00b_cstring("Mul"));
    n00b_pitem_t   *paren   = n00b_pitem_nonterm_raw(grammar,
                                                 n00b_cstring("Paren"));
    n00b_pitem_t   *digit   = n00b_pitem_builtin_raw(N00B_P_BIC_DIGIT);
    n00b_nonterm_t *nt_a    = n00b_pitem_get_ruleset(grammar, add);
    n00b_nonterm_t *nt_m    = n00b_pitem_get_ruleset(grammar, mul);
    n00b_nonterm_t *nt_p    = n00b_pitem_get_ruleset(grammar, paren);
    n00b_list_t    *rule1a  = n00b_list(n00b_type_ref());
    n00b_list_t    *rule1b  = n00b_list(n00b_type_ref());
    n00b_list_t    *rule2a  = n00b_list(n00b_type_ref());
    n00b_list_t    *rule2b  = n00b_list(n00b_type_ref());
    n00b_list_t    *rule3a  = n00b_list(n00b_type_ref());
    n00b_list_t    *rule3b  = n00b_list(n00b_type_ref());
    n00b_list_t    *plmi    = n00b_list(n00b_type_ref());
    n00b_list_t    *mudv    = n00b_list(n00b_type_ref());
    n00b_list_t    *lgrp    = n00b_list(n00b_type_ref());

    n00b_list_append(plmi, n00b_pitem_terminal_cp('+'));
    n00b_list_append(plmi, n00b_pitem_terminal_cp('-'));
    n00b_list_append(mudv, n00b_pitem_terminal_cp('*'));
    n00b_list_append(mudv, n00b_pitem_terminal_cp('/'));

    // Add -> Add [+-] Mul
    n00b_list_append(rule1a, add);
    n00b_list_append(rule1a, n00b_pitem_choice_raw(grammar, plmi));
    n00b_list_append(rule1a, mul);

    // Add -> Mul
    n00b_list_append(rule1b, mul);

    // Mul -> Mul [*/] Paren
    n00b_list_append(rule2a, mul);
    n00b_list_append(rule2a, n00b_pitem_choice_raw(grammar, mudv));
    n00b_list_append(rule2a, paren);

    // Mul -> Paren
    n00b_list_append(rule2b, paren);

    // Paren '(' Add ')'
    n00b_list_append(rule3a, n00b_pitem_terminal_cp('('));
    n00b_list_append(rule3a, add);
    n00b_list_append(rule3a, n00b_pitem_terminal_cp(')'));

    // Paren -> [0-9]+
    n00b_list_append(lgrp, digit);
    n00b_list_append(rule3b, n00b_group_items(grammar, lgrp, 1, 0));

    n00b_ruleset_add_rule(grammar, nt_a, rule1a, 0);
    n00b_ruleset_add_rule(grammar, nt_a, rule1b, 0);
    n00b_ruleset_add_rule(grammar, nt_m, rule2a, 0);
    n00b_ruleset_add_rule(grammar, nt_m, rule2b, 0);
    n00b_ruleset_add_rule(grammar, nt_p, rule3a, 0);
    n00b_ruleset_add_rule(grammar, nt_p, rule3b, 0);

    n00b_list_t *high_cost_rule = n00b_list(n00b_type_ref());
    n00b_list_append(high_cost_rule, n00b_new_pitem(N00B_P_ANY));
    n00b_list_append(high_cost_rule, digit);
    n00b_list_append(high_cost_rule, n00b_new_pitem(N00B_P_ANY));
    n00b_ruleset_add_rule(grammar, nt_m, high_cost_rule, 100);

    n00b_table_t  *unmunged_grammar = n00b_grammar_format(grammar);
    n00b_parser_t *parser           = n00b_new(n00b_type_parser(), grammar);

    parser->tree_annotations = true;

    one_parse(parser, "21");
    one_parse(parser, "1+2");
    one_parse(parser, "(1)");
    one_parse(parser, "(1+2)");
    one_parse(parser, "1+(2+3)");
    one_parse(parser, "1+(2*3+4567)");
    one_parse(parser, " (1)");
    one_parse(parser, " 1");
    one_parse(parser, "1)");
    one_parse(parser, " 1)");

    grammar->hide_penalty_rewrites = false;
    n00b_table_t *munged_grammar   = n00b_grammar_format(grammar);

    n00b_print(n00b_call_out(n00b_crich(
        "«em1»Grammar used for above parses")));
    n00b_print(unmunged_grammar);

    n00b_printf("«em2»Grammar With all rewrites:");
    n00b_print(munged_grammar);
}

static void
show_gopt_results(n00b_gopt_ctx *gopt, n00b_list_t *all_parses)
{
    int num_parses = n00b_list_len(all_parses);
    n00b_printf("«em3»«#»«/» accepted parse(s).", num_parses);

    for (int i = 0; i < num_parses; i++) {
        n00b_gopt_result_t *res = n00b_list_get(all_parses, i, NULL);

        if (n00b_list_len(res->errors)) {
            n00b_printf("«em2»Errors in getopt:");
            for (int j = 0; j < n00b_list_len(res->errors); j++) {
                n00b_print(n00b_list_get(res->errors, j, NULL));
            }
            return;
        }

        n00b_printf("«em2»Parse «#» Command:«/» «em2»«#»", i + 1, res->cmd);
        uint64_t n;

        assert(res->args);

        n00b_string_t **arg_keys = (void *)hatrack_dict_keys_sort(res->args, &n);
        bool            got_args = false;

        for (unsigned int j = 0; j < n; j++) {
            n00b_string_t *s    = arg_keys[j];
            n00b_list_t   *args = hatrack_dict_get(res->args, s, NULL);

            if (args != NULL && n00b_list_len(args)) {
                got_args = true;

                if (!n00b_string_codepoint_len(s)) {
                    s = n00b_cstring("Root chalk command");
                }

                n00b_obj_t obj = n00b_clean_internal_list(args);

                n00b_printf("«em3»«#» args for [reverse]«#»«/»: «em2»«#»«/»",
                            n00b_list_len(args),
                            s,
                            obj);
            }
        }
        if (!got_args) {
            n00b_printf("«em4»No subcommands took arguments.");
        }

        int64_t *flag_info = (int64_t *)hatrack_dict_keys_sort(res->flags, &n);
        for (unsigned int j = 0; j < n; j++) {
            int64_t           key    = flag_info[j];
            n00b_rt_option_t *option = hatrack_dict_get(res->flags,
                                                        (void *)key,
                                                        NULL);
            n00b_printf("«em3»Flag «#»: «/» «#»",
                        option->spec->name,
                        option->value);
        }
    }
}

static void
_gopt_test(n00b_gopt_ctx *gopt, n00b_list_t *args)
{
    n00b_list_t *res;
    res = n00b_gopt_parse(gopt, n00b_cstring("chalk"), args);
    show_gopt_results(gopt, res);
    n00b_printf("«em1»Run command was: chalk «#»", args);
}

#define gopt_test(g, ...)                         \
    {                                             \
        n00b_list_t *l = n00b_c_map(__VA_ARGS__); \
        _gopt_test(g, l);                         \
    }

n00b_gopt_ctx *
setup_gopt_test(void)
{
    n00b_gopt_ctx   *gopt  = n00b_new(n00b_type_gopt_parser(),
                                   N00B_TOPLEVEL_IS_ARGV0);
    n00b_gopt_cspec *chalk = n00b_new(n00b_type_gopt_command(),
                                      n00b_kw("context", n00b_ka(gopt)));

    n00b_new(n00b_type_gopt_option(),
             n00b_kw("name",
                     n00b_cstring("color"),
                     "linked_command",
                     chalk,
                     "opt_type",
                     N00B_GOAT_BOOL_T_DEFAULT));

    n00b_new(n00b_type_gopt_option(),
             n00b_kw("name",
                     n00b_cstring("no-color"),
                     "linked_command",
                     chalk,
                     "opt_type",
                     N00B_GOAT_BOOL_F_DEFAULT));

    n00b_new(n00b_type_gopt_option(),
             n00b_kw("name",
                     n00b_cstring("testflag"),
                     "linked_command",
                     chalk,
                     "opt_type",
                     N00B_GOAT_WORD,
                     "max_args",
                     n00b_ka(0)));

    n00b_gopt_cspec *insert = n00b_new(n00b_type_gopt_command(),
                                       n00b_kw("context",
                                               n00b_ka(gopt),
                                               "name",
                                               n00b_ka(n00b_cstring("insert")),
                                               "parent",
                                               n00b_ka(chalk)));

    n00b_gopt_add_subcommand(gopt, insert, n00b_cstring("(STR)*"));

    n00b_gopt_cspec *extract = n00b_new(n00b_type_gopt_command(),
                                        n00b_kw("context",
                                                n00b_ka(gopt),
                                                "name",
                                                n00b_ka(n00b_cstring("extract")),
                                                "parent",
                                                n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, extract, n00b_cstring("(str)*"));

    /*n00b_gopt_cspec *images     =*/n00b_new(n00b_type_gopt_command(),
                                              n00b_kw("context",
                                                      n00b_ka(gopt),
                                                      "name",
                                                      n00b_ka(n00b_cstring("images")),
                                                      "parent",
                                                      n00b_ka(extract)));
    /*n00b_gopt_cspec *containers =*/n00b_new(n00b_type_gopt_command(),
                                              n00b_kw("context",
                                                      n00b_ka(gopt),
                                                      "name",
                                                      n00b_ka(n00b_cstring("containers")),
                                                      "parent",
                                                      n00b_ka(extract)));

    n00b_gopt_cspec *extract_all = n00b_new(n00b_type_gopt_command(),
                                            n00b_kw("context",
                                                    n00b_ka(gopt),
                                                    "name",
                                                    n00b_ka(n00b_cstring("all")),
                                                    "parent",
                                                    n00b_ka(extract)));
    n00b_gopt_add_subcommand(gopt, extract_all, n00b_cstring("(str)*"));

    n00b_gopt_cspec *del = n00b_new(n00b_type_gopt_command(),
                                    n00b_kw("context",
                                            n00b_ka(gopt),
                                            "name",
                                            n00b_ka(n00b_cstring("delete")),
                                            "parent",
                                            n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, del, n00b_cstring("(str)*"));

    n00b_gopt_cspec *env = n00b_new(n00b_type_gopt_command(),
                                    n00b_kw("context",
                                            n00b_ka(gopt),
                                            "name",
                                            n00b_ka(n00b_cstring("env")),
                                            "parent",
                                            n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, env, n00b_cstring("(str)*"));

    n00b_gopt_cspec *exec = n00b_new(n00b_type_gopt_command(),
                                     n00b_kw("context",
                                             n00b_ka(gopt),
                                             "name",
                                             n00b_ka(n00b_cstring("exec")),
                                             "bad_opt_passthrough",
                                             n00b_ka(true),
                                             "parent",
                                             n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, exec, n00b_cstring("(str)*"));

    /*n00b_gopt_cspec *config =*/n00b_new(n00b_type_gopt_command(),
                                          n00b_kw("context",
                                                  n00b_ka(gopt),
                                                  "name",
                                                  n00b_ka(n00b_cstring("config")),
                                                  "parent",
                                                  n00b_ka(chalk)));

    n00b_gopt_cspec *dump = n00b_new(n00b_type_gopt_command(),
                                     n00b_kw("context",
                                             n00b_ka(gopt),
                                             "name",
                                             n00b_ka(n00b_cstring("dump")),
                                             "parent",
                                             n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, dump, n00b_cstring("(str)?"));

    /*n00b_gopt_cspec *params =*/n00b_new(n00b_type_gopt_command(),
                                          n00b_kw("context",
                                                  n00b_ka(gopt),
                                                  "name",
                                                  n00b_ka(n00b_cstring("params")),
                                                  "parent",
                                                  n00b_ka(dump)));
    /*n00b_gopt_cspec *cache = */ n00b_new(n00b_type_gopt_command(),
                                           n00b_kw("context",
                                                   n00b_ka(gopt),
                                                   "name",
                                                   n00b_ka(n00b_cstring("cache")),
                                                   "parent",
                                                   n00b_ka(dump)));
    /*n00b_gopt_cspec *dump_all =*/n00b_new(n00b_type_gopt_command(),
                                            n00b_kw("context",
                                                    n00b_ka(gopt),
                                                    "name",
                                                    n00b_ka(n00b_cstring("all")),
                                                    "parent",
                                                    n00b_ka(dump)));
    n00b_gopt_cspec *load = n00b_new(n00b_type_gopt_command(),
                                     n00b_kw("context",
                                             n00b_ka(gopt),
                                             "name",
                                             n00b_ka(n00b_cstring("load")),
                                             "parent",
                                             n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, load, n00b_cstring("(str)?"));

    /*n00b_gopt_cspec *version =*/n00b_new(n00b_type_gopt_command(),
                                           n00b_kw("context",
                                                   n00b_ka(gopt),
                                                   "name",
                                                   n00b_ka(n00b_cstring("version")),
                                                   "parent",
                                                   n00b_ka(chalk)));

    n00b_gopt_cspec *docker = n00b_new(n00b_type_gopt_command(),
                                       n00b_kw("context",
                                               n00b_ka(gopt),
                                               "name",
                                               n00b_ka(n00b_cstring("docker")),
                                               "parent",
                                               n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, docker, n00b_cstring("raw"));

    /*n00b_gopt_cspec *setup =*/n00b_new(n00b_type_gopt_command(),
                                         n00b_kw("context",
                                                 n00b_ka(gopt),
                                                 "name",
                                                 n00b_ka(n00b_cstring("setup")),
                                                 "parent",
                                                 n00b_ka(chalk)));

    /*n00b_gopt_cspec *docgen =*/n00b_new(n00b_type_gopt_command(),
                                          n00b_kw("context",
                                                  n00b_ka(gopt),
                                                  "name",
                                                  n00b_ka(n00b_cstring("docgen")),
                                                  "parent",
                                                  n00b_ka(chalk)));
    /*n00b_gopt_cspec *__ =*/n00b_new(n00b_type_gopt_command(),
                                      n00b_kw("context",
                                              n00b_ka(gopt),
                                              "name",
                                              n00b_ka(n00b_cstring("__")),
                                              "parent",
                                              n00b_ka(chalk)));

    n00b_gopt_cspec *numtest = n00b_new(n00b_type_gopt_command(),
                                        n00b_kw("context",
                                                n00b_ka(gopt),
                                                "name",
                                                n00b_ka(n00b_cstring("num")),
                                                "parent",
                                                n00b_ka(chalk)));
    n00b_gopt_add_subcommand(gopt, numtest, n00b_cstring("(float int) *"));

    n00b_new(n00b_type_gopt_option(),
             n00b_kw("name",
                     n00b_cstring("foobar"),
                     "linked_command",
                     numtest,
                     "opt_type",
                     N00B_GOAT_WORD,
                     "max_args",
                     n00b_ka(1)));

    return gopt;
}

void
test_markdown(void)
{
    n00b_string_t *s = n00b_cstring(
        "# h1 \n"
        "Intro to stuff.\n"
        "  1. Ordered 1\n"
        "  2. Ordered 2\n"
        "\n"
        "- Bullet\n"
        "- *hello*\n"
        "- __sup__\n\n"
        "Some text.\n"
        "Some more text.\n\n"
        "Even more text.\n");
    n00b_print(n00b_repr_md_parse(n00b_parse_markdown(s)));
    n00b_print(n00b_markdown_to_table(s, true));
}

void
test_getopt(void)
{
    n00b_gopt_ctx *gopt = setup_gopt_test();

    // gopt->show_debug = true;

    gopt_test(gopt, "--color", "False", "version");
    gopt_test(gopt, "--color", "f", "--color", "False", "version");
    gopt_test(gopt, "extract", "foo", "--color", "true");
    gopt_test(gopt, "load", "foo", "--color", "true");
    gopt_test(gopt, "--color", "load", "foo");
    gopt_test(gopt, "--color", "num", "1209238", "37");
    gopt_test(gopt,
              "extract",
              "--color=",
              "true",
              "foo",
              "bar",
              "bleep",
              "74");

    gopt_test(gopt,
              "--color",
              "extract",
              "--no-color",
              "containers",
              "--color=",
              "true");
    gopt_test(gopt,
              "--color",
              "extract",
              "--no-color",
              "containers",
              "--testflag=x,y ",
              ",z");
    gopt_test(gopt, "--unknown", "foo", "version");
    gopt_test(gopt, "num", "12", "foo");
    gopt_test(gopt, "num", "12");
    gopt_test(gopt, "load", "foo", "bar");
    gopt_test(gopt, "--foobar", "x", "num", "1", "2");
    gopt_test(gopt, "--foobar", "x", "extract", "foo", "bar");
    gopt_test(gopt, "num", "1", "2", "--foobar");
    gopt_test(gopt, "--foobar", "num", "1", "2");
    gopt_test(gopt, "--foobar");

    n00b_print(n00b_call_out(n00b_cstring("Grammar used for above parses")));
    n00b_print(n00b_grammar_format(gopt->grammar));
}

int
main(int argc, char **argv, char **envp)
{
    n00b_init(argc, argv, envp);
    n00b_terminal_app_setup();

    add_static_test_symbols();
    n00b_terminal_dimensions(&n00b_term_width, NULL);

    if (n00b_get_env(n00b_cstring("N00B_DEV"))) {
        n00b_dev_mode = true;
    }

    n00b_scan_and_prep_tests();
    n00b_run_expected_value_tests();
    n00b_run_other_test_files();

    // test_parsing();
    // test_getopt();
    // test_markdown();

    n00b_report_results_and_exit();
    n00b_unreachable();
    return 0;
}
