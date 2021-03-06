#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include "common.h"
#include "engine.h"

#include <unicode/ubrk.h>

enum {
    UCUT_EXIT_SUCCESS = 0,
    UCUT_EXIT_FAILURE,
    UCUT_EXIT_USAGE
};

/* ========== global variables ========== */

extern engine_t fixed_engine;
extern engine_t re_engine;

static UBool cFlag = FALSE;
static UBool fFlag = FALSE;
static UBool sFlag = FALSE;

static const UChar DEFAULT_DELIM[] = { U_HT, 0 };

static UString *ustr = NULL;
static UBreakIterator *ubrk = NULL;
static UString *input_delim = NULL;
static UString *output_delim = NULL;
static interval_list_t *intervals = NULL;

static DArray *pieces = NULL;
static pattern_data_t pdata = { NULL, &fixed_engine };

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC,
    OUTPUT_DELIMITER_OPT,
    COMPLEMENT_OPT
};

static char optstr[] = "EFb:c:d:f:nvs";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"extended-regexp",  no_argument,       NULL, 'E'}, // grep
    {"fixed-string",     no_argument,       NULL, 'F'}, // grep
    {"bytes",            required_argument, NULL, 'b'},
    {"characters",       required_argument, NULL, 'c'},
    {"complement",       no_argument,       NULL, COMPLEMENT_OPT},
    {"delimiter",        required_argument, NULL, 'd'},
    {"fields",           required_argument, NULL, 'f'},
    {"version",          no_argument,       NULL, 'v'},
    {"only-delimited",   no_argument,       NULL, 's'},
    {"output-delimiter", required_argument, NULL, OUTPUT_DELIMITER_OPT},
    {NULL,               no_argument,       NULL, 0}
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

static UBool ubrk_fwd_n(UBreakIterator *ubrk, size_t n, int32_t *r)
{
    while (n > 0 && UBRK_DONE != (*r = ubrk_next(ubrk))) {
        --n;
    }

    return (0 == n);
}

static int32_t split_on_indices(error_t **error, UBreakIterator *ubrk, UString *ustr, DArray *array, interval_list_t *intervals)
{
    dlist_element_t *el;
    int32_t pieces, l, u, lastU;

    lastU = pieces = l = u = 0;
    if (NULL == ubrk) {
        for (el = intervals->head; NULL != el && (size_t) u < ustr->len; el = el->next) {
            FETCH_DATA(el->data, i, interval_t);

            if (i->lower_limit > 0) {
                U16_FWD_N(ustr->ptr, l, ustr->len, i->lower_limit - lastU);
                u = l;
            }
            U16_FWD_N(ustr->ptr, u, ustr->len, i->upper_limit - i->lower_limit);
            add_match(array, ustr, l, u);
            ++pieces;
            lastU = i->upper_limit;
            l = u;
        }
    } else {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ubrk_setText(ubrk, ustr->ptr, ustr->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_setText");
            return -1;
        }
        if (UBRK_DONE != (l = ubrk_first(ubrk))) {
            for (el = intervals->head; NULL != el && UBRK_DONE != u; el = el->next) {
                FETCH_DATA(el->data, i, interval_t);

                if (i->lower_limit > 0) {
                    if (!ubrk_fwd_n(ubrk, i->lower_limit - lastU, &l)) {
                        break;
                    }
                }
                if (!ubrk_fwd_n(ubrk, i->upper_limit - i->lower_limit, &u)) {
                    break;
                }
                add_match(array, ustr, l, u);
                ++pieces;
                lastU = i->upper_limit;
                l = u;
            }
        }
        if (UBRK_DONE != l && UBRK_DONE == u && (size_t) l < ustr->len) {
            add_match(array, ustr, l, ustr->len);
            ++pieces;
        }
        ubrk_setText(ubrk, NULL, 0, &status);
        assert(U_SUCCESS(status));
    }

    return pieces;
}

/* ========== main ========== */

static int procfile(reader_t *reader, const char *filename)
{
    error_t *error;
    int32_t j, count;

    error = NULL;
    if (reader_open(reader, &error, filename)) {
        while (!reader_eof(reader)) {
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            ustring_chomp(ustr);
            darray_clear(pieces);
            if (fFlag) {
                if (!pdata.engine->split(&error, pdata.pattern, ustr, pieces, intervals)) {
                    print_error(error);
                    return 1;
                }
                count = darray_length(pieces);
            } else if (cFlag) {
                count = split_on_indices(&error, ubrk, ustr, pieces, intervals);
            } else {
                assert(FALSE);
            }
            if (count > 0) {
                for (j = 0; j < count; j++) {
                    match_t m;

                    m = darray_at_unsafe(pieces, j, match_t);
                    if (NULL != output_delim && 0 != j) {
                        u_file_write(output_delim->ptr, output_delim->len, ustdout);
                    }
// debug(">%.*S< (%d) >%S<", m->len, m->ptr, m->len, m->ptr);
                    u_file_write(m.ptr, m.len, ustdout);
                }
                u_file_write(EOL, EOL_LEN, ustdout);
            } else if (!sFlag && fFlag) {
                u_file_write(ustr->ptr, ustr->len, ustdout);
                u_file_write(EOL, EOL_LEN, ustdout);
            }
        }
        reader_close(reader);
    } else {
        print_error(error);
        return 1;
    }

    return 0;
}

// ./ucut -sd a -f 1 test/data/ucut.txt => is incorrect? (depends on output delim?)
int main(int argc, char **argv)
{
    int c, ret;
    error_t *error;
    reader_t *reader;
    UBool complement;
    const char *intervals_arg, *input_delim_arg, *output_delim_arg;

    ret = 0;
    error = NULL;
    complement = FALSE;
    intervals = interval_list_new();
    env_init(UCUT_EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);
    intervals_arg = input_delim_arg = output_delim_arg = NULL;

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'E':
                pdata.engine = &re_engine;
                break;
            case 'F':
                pdata.engine = &fixed_engine;
                break;
            case 'b':
                fprintf(stderr, "Working with bytes makes no sense: %s works in UTF-16, after a possible charset conversion and normalization\n", __progname);
                return UCUT_EXIT_FAILURE;
            case 'c':
                cFlag = TRUE;
                intervals_arg = optarg;
                break;
            case 'd':
                input_delim_arg = optarg;
                break;
            case 'f':
                fFlag = TRUE;
                intervals_arg = optarg;
                break;
            case 'n':
                /* NOP: ignore, compatibility only */
                break;
            case 's':
                sFlag = TRUE;
                break;
            case COMPLEMENT_OPT:
                complement = TRUE;
                break;
            case OUTPUT_DELIMITER_OPT:
                output_delim_arg = optarg;
                break;
            default:
                if (!util_opt_parse(c, optarg, reader)) {
                    usage();
                }
                break;
        }
    }
    argc -= optind;
    argv += optind;

    env_apply();

    if (cFlag && (fFlag || NULL != input_delim_arg)) {
        usage();
    }
    if (!cFlag && !fFlag) {
        usage();
    }
    if (cFlag && UNIT_GRAPHEME == env_get_unit()) {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(&error, FATAL, status, "ubrk_open");
            print_error(error);
        }
        env_register_resource(ubrk, (func_dtor_t) ubrk_close);
    }
    if (!parseIntervals(&error, intervals_arg, intervals, 1)) {
        print_error(error);
    }
    if (complement) {
        interval_list_complement(intervals, 0, INT32_MAX);
    }
    if (interval_list_empty(intervals)) {
        fprintf(stderr, "non empty list of fields, characters or bytes expected\n");
        return UCUT_EXIT_USAGE;
    }
#if defined(DEBUG) && 0
    interval_list_debug(intervals);
#endif
    env_register_resource(intervals, (func_dtor_t) interval_list_destroy);
    if (fFlag) {
        if (NULL == input_delim_arg) {
            input_delim = ustring_dup_string_len(DEFAULT_DELIM, STR_LEN(DEFAULT_DELIM));
        } else {
            if (NULL == (input_delim = ustring_convert_argv_from_local(input_delim_arg, &error, TRUE))) {
                print_error(error);
            }
        }
        //env_register_resource(input_delim, (func_dtor_t) ustring_destroy); // done by the engine?
        if (NULL == (pdata.pattern = pdata.engine->compile(&error, input_delim, 0))) {
            print_error(error);
        } else {
            env_register_resource(pdata.pattern, pdata.engine->destroy);
        }
        if (NULL == output_delim_arg && pdata.engine != &re_engine) {
            output_delim = input_delim; /* do not register output_delim */
        }
    }
    if (NULL != output_delim_arg) {
        if (NULL == (output_delim = ustring_convert_argv_from_local(output_delim_arg, &error, TRUE))) {
            print_error(error);
        }
        env_register_resource(output_delim, (func_dtor_t) ustring_destroy);
    }
    /*if (pdata.engine == &re_engine && NULL == output_delim_arg) {
        fprintf(stderr, "Using regular expression implies an output-delimiter\n");
        return UCUT_EXIT_USAGE;
    }*/
    ustr = ustring_new();
    env_register_resource(ustr, (func_dtor_t) ustring_destroy);
    pieces = darray_sized_new(interval_list_length(intervals), sizeof(match_t));
    env_register_resource(pieces, (func_dtor_t) darray_destroy);

    if (0 == argc) {
        ret |= procfile(reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(reader, *argv);
        }
    }

    return (0 == ret ? UCUT_EXIT_SUCCESS : UCUT_EXIT_FAILURE);
}
