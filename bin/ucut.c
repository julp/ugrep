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

#include <unicode/ubrk.h>

enum {
    UCUT_EXIT_SUCCESS = 0,
    UCUT_EXIT_FAILURE,
    UCUT_EXIT_USAGE
};

enum {
    FIELD_NO_ERR = 0,
    FIELD_ERR_NUMBER_EXPECTED, // s == *endptr
    FIELD_ERR_OUT_OF_RANGE,    // number not in [min;max] ([1;INT_MAX] here)
    FIELD_ERR_NON_DIGIT_FOUND, // *endptr not in ('\0', ',')
    FIELD_ERR_INVALID_RANGE,   // lower_limit > upper_limit
    FIELD_ERR__COUNT
};

/* ========== global variables ========== */

extern engine_t fixed_engine;
extern engine_t re_engine;

static UBool cFlag = FALSE;
static UBool fFlag = FALSE;
static UBool sFlag = FALSE;

static const UChar DEFAULT_DELIM[] = { U_HT, 0 };

static UString *ustr = NULL;
static UString *delim = NULL;
static UBreakIterator *ubrk = NULL;
static interval_list_t *intervals = NULL;

static DPtrArray *pieces = NULL;
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
    {"output-delimiter", no_argument,       NULL, OUTPUT_DELIMITER_OPT},
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

UBool ubrk_fwd_n(UBreakIterator *ubrk, size_t n, int32_t *r)
{
    while (n > 0 && UBRK_DONE != (*r = ubrk_next(ubrk))) {
        --n;
    }

    return (0 == n);
}

static int32_t split_on_indices(error_t **error, UBreakIterator *ubrk, UString *ustr, DPtrArray *array, interval_list_t *intervals)
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
            for (el = intervals->head; NULL != el && u != UBRK_DONE; el = el->next) {
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
        if (!pieces) {
//         add_match(array, ustr, 0, ustr->len);
//         ++pieces;
        } else if (UBRK_DONE != l && UBRK_DONE == u && (size_t) l < ustr->len) {
            add_match(array, ustr, l, ustr->len);
            ++pieces;
        }
        ubrk_setText(ubrk, NULL, 0, &status);
        assert(U_SUCCESS(status));
    }

    return pieces;
}

/* ========== main ========== */

static const char *intervalParsingErrorName(int code)
{
    switch (code) {
        case FIELD_NO_ERR:
            return "no error";
        case FIELD_ERR_NUMBER_EXPECTED:
            return "number expected";
        case FIELD_ERR_OUT_OF_RANGE:
            return "number is out of the range [1;INT32_MAX[";
        case FIELD_ERR_NON_DIGIT_FOUND:
            return "non digit character found";
        case FIELD_ERR_INVALID_RANGE:
            return "invalid range: upper limit should be greater or equal than lower limit";
        default:
            return "bogus error code";
    }
}

#ifndef HAVE_STRCHRNUL
static char *strchrnul(const char *s, int c)
{
    while (('\0' != *s) && (*s != c)) {
        s++;
    }

    return (char *) s;
}
#endif /* HAVE_STRCHRNUL */

static int parseIntervalBoundary(const char *nptr, char **endptr, int32_t min, int32_t max, int32_t *ret)
{
    char c;
    const char *s;
    UBool negative;
    int any, cutlim;
    uint32_t cutoff, acc;

    s = nptr;
    acc = any = 0;
    if ('-' == (c = *s++)) {
        negative = TRUE;
        c = *s++;
    } else {
        negative = FALSE;
        if ('+' == *s) {
            c = *s++;
        }
    }
    cutoff = negative ? (uint32_t) - (INT32_MIN + INT32_MAX) + INT32_MAX : INT32_MAX;
    cutlim = cutoff % 10;
    cutoff /= 10;
    do {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else {
            break;
        }
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= 10;
            acc += c;
        }
    } while ('\0' != (c = *s++));
    if (NULL != endptr) {
        *endptr = (char *) (any ? s - 1 : nptr);
    }
    if (any < 0) {
        *ret = negative ? INT32_MIN : INT32_MAX;
        return FIELD_ERR_OUT_OF_RANGE;
    } else if (!any) {
        return FIELD_ERR_NUMBER_EXPECTED;
    } else if (negative) {
        *ret = -acc;
    } else {
        *ret = acc;
    }
    if (*ret < min || *ret > max) {
        *endptr = (char *) nptr;
        return FIELD_ERR_OUT_OF_RANGE;
    }

    return FIELD_NO_ERR;
}

/**
 * The elements in list can be repeated, can overlap, and can be specified in any order,
 * but the bytes, characters, or fields selected shall be written in the order of the
 * input data. If an element appears in the selection list more than once, it shall be
 * written exactly once.
 **/
static UBool parseIntervals(error_t **error, const char *s, interval_list_t *intervals)
{
    int ret;
    char *endptr;
    const char *p, *comma;
    int32_t lower_limit, upper_limit;

    p = s;
    while ('\0' != *p) {
        lower_limit = 1;
        upper_limit = INT32_MAX;
        comma = strchrnul(p, ',');
        if ('-' == *p) {
            /* -Y */
            if (0 != (ret = parseIntervalBoundary(p + 1, &endptr, 1, INT32_MAX, &upper_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                return FALSE;
            }
        } else {
            if (NULL == memchr(p, '-', comma - p)) {
                /* X */
                if (0 != (ret = parseIntervalBoundary(p, &endptr, 1, INT32_MAX, &lower_limit))/* || ('\0' != *endptr && ',' != *endptr)*/) {
                    error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                    return FALSE;
                }
                if ('\0' != *endptr && ',' != *endptr) {
                    error_set(error, FATAL, "digit or delimiter expected:\n%s\n%*c", s, endptr - s + 1, '^');
                    return FALSE;
                }
                upper_limit = lower_limit;
            } else {
                /* X- or X-Y */
                if (0 != (ret = parseIntervalBoundary(p, &endptr, 1, INT32_MAX, &lower_limit))) {
                    error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                    return FALSE;
                }
                if ('-' == *endptr) {
                    if ('\0' == *(endptr + 1)) {
                        // NOP (lower_limit = 0)
                    } else {
                        if (0 != (ret = parseIntervalBoundary(endptr + 1, &endptr, 1, INT32_MAX, &upper_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                            error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                            return FALSE;
                        }
                    }
                } else {
                    error_set(error, FATAL, "'-' expected, get '%c' (%d):\n%s\n%*c", *endptr, *endptr, s, endptr - s + 1, '^');
                    return FALSE;
                }
            }
            if (lower_limit > upper_limit) {
                error_set(error, FATAL, "invalid interval: lower limit greater then upper one:\n%s\n%*c", s, p - s + 1, '^');
                return FALSE;
            }
        }
        debug("add [%d;%d[", lower_limit - 1, upper_limit);
        interval_list_add(intervals, INT32_MAX, lower_limit - 1, upper_limit); // - 1 because first index is 0 not 1
#if defined(DEBUG) && 0
        interval_list_debug(intervals);
#endif
        if ('\0' == *comma) {
            break;
        }
        p = comma + 1;
    }

    return TRUE;
}

static int procfile(reader_t *reader, const char *filename)
{
    int32_t j, count;
    error_t *error;
    dlist_element_t *el;

    error = NULL;
    dptrarray_clear(pieces);
    if (reader_open(reader, &error, filename)) {
        while (!reader_eof(reader)) {
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            ustring_chomp(ustr);
            if (fFlag) {
                count = pdata.engine->split(&error, pdata.pattern, ustr, pieces);
            } else if (cFlag) {
                count = split_on_indices(&error, ubrk, ustr, pieces, intervals);
            } else {
                assert(FALSE);
            }
            if (count < 0) {
                print_error(error);
            } else if (count > 0) {
                if (fFlag) {
                    for (el = intervals->head; NULL != el; el = el->next) {
                        FETCH_DATA(el->data, i, interval_t);

                        for (j = i->lower_limit; j < MIN(count, i->upper_limit); j++) {
                            match_t *m;

                            m = dptrarray_at(pieces, j);
                            u_file_write(m->ptr, m->len, ustdout);
                            //u_file_write(delim->ptr, delim->len, ustdout); // TODO: already freed by RE engine ; can we make it dynamic (capture)?
                            //u_fprintf(ustdout, "%d: %.*S (%d)\n", j + 1, m->len, m->ptr, m->len);
                        }
                    }
                } else if (cFlag) {
                    for (j = 0; j < count; j++) {
                        match_t *m;

                        m = dptrarray_at(pieces, j);
                        u_file_write(m->ptr, m->len, ustdout);
                    }
                } else {
                    assert(FALSE);
                }
                u_file_write(EOL, EOL_LEN, ustdout);
            } else if (!sFlag) {
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
    reader_t reader;
    UBool complement;
    const char *intervals_arg, *delim_arg;

    ret = 0;
    error = NULL;
    complement = FALSE;
    intervals = interval_list_new();
    intervals_arg = delim_arg = NULL;
    env_init();
    reader_init(&reader, DEFAULT_READER_NAME);
    exit_failure_value = UCUT_EXIT_FAILURE;

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'E':
                pdata.engine = &re_engine;
                break;
            case 'F':
                pdata.engine = &fixed_engine;
                break;
            case 'b':
                fputs("Working with bytes makes no sense: ucut works in UTF-16, after a possible charset conversion and normalization", stderr);
                return UCUT_EXIT_FAILURE;
            case 'c':
                cFlag = TRUE;
                intervals_arg = optarg;
                break;
            case 'd':
                delim_arg = optarg;
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

    if (cFlag && fFlag) {
        usage();
    }
    if (!cFlag && !fFlag) {
        usage();
    }
    if (cFlag && UNORM_NONE != env_get_normalization()) {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(&error, FATAL, status, "ubrk_open");
            print_error(error);
        }
    }
    if (!parseIntervals(&error, intervals_arg, intervals)) {
        print_error(error);
    }
    if (complement) {
        interval_list_complement(intervals, 0, INT32_MAX);
#if 1
        if (interval_list_empty(intervals)) {
            fprintf(stderr, "non empty list of fields, characters or bytes expected\n");
        }
#endif
    }
#if defined(DEBUG) && 0
    interval_list_debug(intervals);
#endif
    env_register_resource(intervals, (func_dtor_t) interval_list_destroy);
    if (NULL == delim_arg) {
        delim = ustring_dup_string_len(DEFAULT_DELIM, STR_LEN(DEFAULT_DELIM));
    } else {
        if (NULL == (delim = ustring_convert_argv_from_local(delim_arg, &error, TRUE))) {
            print_error(error);
        }
    }
    if (fFlag) {
        if (NULL == (pdata.pattern = pdata.engine->compile(&error, delim, 0))) {
            print_error(error);
        } else {
            env_register_resource(pdata.pattern, pdata.engine->destroy);
        }
    }
    ustr = ustring_new();
    env_register_resource(ustr, (func_dtor_t) ustring_destroy);
    pieces = dptrarray_new(free);
    env_register_resource(pieces, (func_dtor_t) dptrarray_destroy);

    if (0 == argc) {
        ret |= procfile(&reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(&reader, *argv);
        }
    }

    return (0 == ret ? UCUT_EXIT_SUCCESS : UCUT_EXIT_FAILURE);
}
