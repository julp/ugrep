#include <getopt.h>

#include "common.h"

#include <unicode/ubrk.h>

typedef enum {
    //BYTES,
    CODEPOINTS = 1<<0,
    GRAPHEMES = 1<<1,
    WORDS = 1<<2,
    LINES = 1<<3,
};

/* ========== getopt stuff ========== */

static char optstr[] = "Lclmw";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"xxx", no_argument, NULL, 'x'},
    {NULL,  no_argument, NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s TODO\n",
        __progname
    );
    exit(/*UWC_*/EXIT_USAGE);
}

/* ========== main ========== */

int main(int argc, char **argv)
{
    int mode;
    int c, ret;
    error_t *error;
    reader_t *reader;

    ret = 0;
    mode = 0;
    error = NULL;
    env_init(/*UWC_*/EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'L':
                // longest line
                break;
            case 'c':
                // unsupported, no sense
                fprintf(stderr, "Working with bytes makes no sense: %s works in UTF-16, after a possible charset conversion and normalization\n", "uwc");
                break;
            case 'l':
                mode |= LINES;
                break;
            case 'm':
                mode |= GRAPHEMES;
                break;
            case 'w':
                mode |= WORDS;
                break;
            default:
                if (!util_opt_parse(c, optarg, &reader)) {
                    usage();
                }
                break;
        }
    }
    argc -= optind;
    argv += optind;

    env_apply();

    // process

    return (0 == ret ? /*UWC_*/EXIT_SUCCESS : /*UWC_*/EXIT_FAILURE);
}
