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

/* ========== global variables ========== */

int from, to;
UString *ustr = NULL;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = CHAR_MAX + 1,
    INPUT_OPT,
    READER_OPT
};

static char optstr[] = "d:f:";

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

/* ========== cutter ========== */

static int /*pieces_length*/ engine_fixed_split(void *data, int from, int to, void/*?*/ *pieces)
{
    // ustring_sync_copy with a ratio of 2 ?
    // dynamic array of UChar * ?
    // list (slist_t) ?
}

/* ========== main ========== */

static int parseInt(const char *s, char **endptr, int *ret)
{
    long val;

    errno = 0;
    val = strtol(s, endptr, 10);
    if (0 != errno || *endptr == s) {
        return 0;
    }
    if (val <= 0/* || val < INT_MIN*/ || val > INT_MAX) {
        return 0;
    }
    *ret = (int) val;

    return 1;
}

static int procfile(reader_t *reader, const char *filename)
{
    error_t *error;

    error = NULL;
    if (reader_open(reader, &error, filename)) {
        while (!reader_eof(reader)) {
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            ustring_chomp(ustr);
            // TODO
        }
        reader_close(reader);
    } else {
        print_error(error);
        return 1;
    }

    return 0;
}

static void exit_cb(void)
{
    if (NULL != ustr) {
        ustring_destroy(ustr);
    }
}

int main(int argc, char **argv)
{
    int c;
    int ret;
    error_t *error;
    reader_t reader;

    if (0 != atexit(exit_cb)) {
        fputs("can't register atexit() callback", stderr);
        return UCUT_EXIT_FAILURE;
    }

    ret = 0;
    error = NULL;
    reader_init(&reader, "mmap");
    exit_failure_value = UCUT_EXIT_FAILURE;

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'd':
                // change delimiter
                break;
            case 'f':
            {
                char *endptr;

                if (*optarg == '-') {
                    /* -Y */
                    // parseInt(optarg+1) ; *endptr = '\0'
                    if (!parseInt(optarg + 1, &endptr, &to) || '\0' != *endptr) {
                        goto invalid_fields;
                    }
                    from = -1;
                } else {
                    if (NULL == strchr(optarg, '-')) {
                        /* X */
                        // parseInt(optarg) ; *endptr = '\0'
                        if (!parseInt(optarg, &endptr, &from) || '\0' != *endptr) {
                            goto invalid_fields;
                        }
                        to = from;
                    } else {
                        /* X- or X-Y */
                        // parseInt(optarg)
                        // *endptr = '-'
                        // optarg++
                        // X-: if (*endptr = '\0') { NOP }
                        // X-Y: else { parseInt(optarg) }
                        if (!parseInt(optarg, &endptr, &from)) {
                            goto invalid_fields;
                        }
                        if ('-' == *endptr) {
                            if ('\0' == *(endptr + 1)) {
                                to = -1;
                            } else {
                                if (!parseInt(endptr + 1, &endptr, &to) || '\0' != *endptr) {
                                    goto invalid_fields;
                                }
                            }
                        } else {
                            goto invalid_fields;
                        }
                    }
                }
                /*if (to != -1 && from > to) {
                    goto invalid_fields;
                }*/
                break;
invalid_fields:
                fprintf(stderr, "Invalid field(s)\n");
                return UCUT_EXIT_USAGE;
            }
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

    ustr = ustring_new();

    if (0 == argc) {
        ret |= procfile(&reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(&reader, *argv);
        }
    }

    return (0 == ret ? UCUT_EXIT_SUCCESS : UCUT_EXIT_FAILURE);
}
