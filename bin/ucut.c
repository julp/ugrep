#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WITHOUT_FTS
# include <fts.h>
#endif /* !WITHOUT_FTS */
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include "common.h"
#include "engine.h"

enum {
    UCUT_EXIT_SUCCESS = 0,
    UCUT_EXIT_FAILURE,
    UCUT_EXIT_USAGE
};

enum {
    FIELD_NO_ERR = 0,
    FIELD_ERR_NUMBER_EXPECTED, // s == *endptr
    FIELD_ERR_OUT_OF_RANGE,    // number <= 0 or number > INT_MAX
    FIELD_ERR_NON_DIGIT_FOUND, // *endptr not in ('\0', ',')
    FIELD_ERR_INVALID_RANGE,   // lower_limit > upper_limit
    FIELD_ERR__COUNT
};

/* ========== global variables ========== */

UBool cFlag = FALSE;
UBool fFlag = FALSE;

const UChar DEFAULT_DELIM[] = { 0x09, 0 };

UString *ustr = NULL;
UString *delim = NULL;

DPtrArray *pieces = NULL;
pattern_data_t *pattern = NULL;

extern engine_t fixed_engine;
extern engine_t re_engine;

engine_t *engines[] = {
    &fixed_engine,
    &re_engine
};

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC
};

static char optstr[] = "d:f:";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"bytes",           required_argument, NULL, 'b'}, // no sense? ignore?
    {"characters",      required_argument, NULL, 'c'},
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

#if 0
static int /*pieces_length*/ split_on_length(void/*?*/ *positions, void/*?*/ *pieces)
{
    // ustring_sync_copy with a ratio of 2 ?
    // dynamic array of UChar * ?
    // list (slist_t) ?
}
#endif

/* ========== main ========== */

#ifndef HAVE_STRCHRNUL
static char *strchrnul(const char *s, int c)
{
    while (('\0' != *s) && (*s != c)) {
        s++;
    }

    return (char *) s;
}
#endif /* HAVE_STRCHRNUL */

static int parseInt(const char *s, char **endptr, int *ret)
{
    long val;

    errno = 0;
    val = strtol(s, endptr, 10);
    if (0 != errno || *endptr == s) {
        return FIELD_ERR_NUMBER_EXPECTED;
    }
    if (val <= 0/* || val < INT_MIN*/ || val > INT_MAX) {
        return FIELD_ERR_OUT_OF_RANGE;
    }
    *ret = (int) val;

    return FIELD_NO_ERR;
}

static int parseFields(const char *s/*, void *positions*/)
{
    const char *p, *comma;
    char *endptr;
    int lower_limit;
    int upper_limit;

    p = s;
    while ('\0' != *p) {
        lower_limit = 0;
        upper_limit = INT_MAX;
        comma = strchrnul(p, ',');
        if ('-' == *p) {
            /* -Y */
            if (parseInt(p + 1, &endptr, &upper_limit) || ('\0' != *endptr && ',' != *endptr)) {
                return 0;
            }
        } else {
            if (NULL == memchr(p, '-', comma - p)) {
                /* X */
                if (parseInt(p, &endptr, &lower_limit) || ('\0' != *endptr && ',' != *endptr)) {
                    return 0;
                }
                upper_limit = lower_limit;
            } else {
                /* X- or X-Y */
                if (parseInt(p, &endptr, &lower_limit)) {
                    return 0;
                }
                if ('-' == *endptr) {
                    if ('\0' == *(endptr + 1)) {
                        // NOP (lower_limit = 0)
                    } else {
                        if (parseInt(endptr + 1, &endptr, &upper_limit) || ('\0' != *endptr && ',' != *endptr)) {
                            return 0;
                        }
                    }
                } else {
                    return 0;
                }
            }
            if (lower_limit >/*=*/ upper_limit) {
                return 0;
            }
        }
        debug("Interval is [%d;%d]", lower_limit, upper_limit);
        if ('\0' == *comma) {
            break;
        }
        p = comma + 1;
    }

    return 1;
}

static int procfile(reader_t *reader, const char *filename)
{
    int32_t i, count;
    error_t *error;

    error = NULL;
    dptrarray_clear(pieces);
    if (reader_open(reader, &error, filename)) {
        while (!reader_eof(reader)) {
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            ustring_chomp(ustr);
            count = fixed_engine.split(&error, pattern, ustr, pieces);
            for (i = 0; i < count; i++) {
                match_t *m;

                m = dptrarray_at(pieces, i);
                //u_file_write(m->ptr, m->len, ustdout);
                u_fprintf(ustdout, "%d: %.*S (%d)\n", i + 1, m->len, m->ptr, m->len);
            }
            u_file_write(EOL, EOL_LEN, ustdout);
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
    if (NULL != pieces) {
        dptrarray_destroy(pieces);
    }
    if (NULL != pattern) {
        fixed_engine.destroy(pattern);
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
    env_init();
    reader_init(&reader, DEFAULT_READER_NAME);
    exit_failure_value = UCUT_EXIT_FAILURE;

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'b':
                debug("parseFields = %d", parseFields(optarg));
                break;
            case 'c':
                cFlag = TRUE;
                debug("parseFields = %d", parseFields(optarg));
                break;
            case 'd':
            {
                /*UString *ustrarg;

                if (NULL == (ustrarg = ustring_convert_argv_from_local(optarg, &error, TRUE))) {
                    print_error(error);
                }
                if (u_strHasMoreChar32Than(ustrarg->ptr, ustrarg->len, 1)) {
                    fprintf(stderr, "Delimiter is not a single character\n");
                    return UCUT_EXIT_FAILURE;
                }
                U16_GET_UNSAFE(ustrarg->ptr, 0, delim);
                ustring_destroy(ustrarg);*/
                if (NULL == (delim = ustring_convert_argv_from_local(optarg, &error, TRUE))) {
                    print_error(error);
                }
                break;
            }
            case 'f':
                fFlag = TRUE;
                debug("parseFields = %d", parseFields(optarg));
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

#if 0
    if (cFlag && fFlag) {
        usage();
    }
    if (!cFlag && !fFlag) {
        usage();
    }
#endif

    if (NULL == delim) {
        delim = ustring_dup_string_len(DEFAULT_DELIM, STR_LEN(DEFAULT_DELIM));
    }
    if (NULL == (pattern = fixed_engine.compile(&error, delim, 0))) {
        print_error(error);
    }
    ustr = ustring_new();
    pieces = dptrarray_new(free);

    if (0 == argc) {
        ret |= procfile(&reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(&reader, *argv);
        }
    }

    return (0 == ret ? UCUT_EXIT_SUCCESS : UCUT_EXIT_FAILURE);
}
