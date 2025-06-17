#include "ncpp.h"

int
main(int argc, char *argv[], char *envp[])
{
    int  base  = 1;
    bool debug = false;

    if (argc > 1) {
        if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--debug")) {
            debug = true;
            base++;
        }
    }

    if (argc != base + 2) {
        fprintf(stderr, "Usage: %s infile outfile\n", argv[0]);
        return -1;
    }

    lex_t state = (lex_t){
        .num_toks   = 0,
        .input      = read_file(argv[base]),
        .toks       = NULL,
        .cur        = NULL,
        .end        = NULL,
        .line_start = true,
        .in_file    = argv[base],
        .out_file   = argv[base + 1],
    };

    if (!state.input) {
        fprintf(stderr,
                "%s: Could not read input file: %s\n",
                argv[0],
                argv[base]);
        return -2;
    }

    if (!strcmp(argv[base], argv[base + 1])) {
        fprintf(stderr,
                "%s: Output file cannot be the same as the input file.",
                argv[0]);
    }

    lex(&state);

    if (debug) {
        print_tokens(&state);
    }

    if (!apply_transforms(&state)) {
        fprintf(stderr, "%s: ncpp translation failed.\n", argv[base]);
        return -3;
    }

    if (!output_new_file(&state, argv[base + 1])) {
        fprintf(stderr,
                "%s: Could not write to file: %s\n",
                argv[0],
                argv[base + 1]);
        return -4;
    }
}
