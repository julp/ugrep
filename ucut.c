#include <limits.h>
#ifndef WITHOUT_FTS
# include <fts.h>
#endif /* !WITHOUT_FTS */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include "common.h"


enum {
    UCUT_EXIT_SUCCESS = 0,
    UCUT_EXIT_FAILURE,
    UCUT_EXIT_USAGE
};

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = CHAR_MAX + 1,
    INPUT_OPT,
    READER_OPT
};

static char optstr[] = "df";

static struct option long_options[] =
{
    {"delimiter",       required_argument, NULL, 'd'},
    {"fields",          required_argument, NULL, 'f'},
    {"version",         no_argument,       NULL, 'v'},
    {NULL,              no_argument,       NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s TODO\n",
        __progname
    );
    exit(UCUT_EXIT_USAGE);
}

int main(int argc, char **argv)
{
    int c;
    error_t *error;
    reader_t reader;

    error = NULL;
    reader_init(&reader, "mmap");
    exit_failure_value = UCUT_EXIT_FAILURE;

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'd':
                // change delimiter
                break;
            case 'f':
                // parse fields option (X, X-, X-Y, -Y)
                break;
            case READER_OPT:
                if (!reader_set_imp_by_name(&reader, optarg)) {
                    fprintf(stderr, "Unknown reader\n");
                    return UCUT_EXIT_USAGE;
                }
                break;
            case INPUT_OPT:
                reader_set_default_encoding(&reader, optarg);
                break;
            default:
                usage();
                break;
        }
    }
    argc -= optind;
    argv += optind;

    //

    reader_close(&reader);

    return UCUT_EXIT_SUCCESS;
}
