#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/uset.h>
#include <unicode/ubrk.h>

#include "common.h"

/**
 * TODO:
 * - consider cFlag (complemente/negate)
 **/

#ifdef DEBUG
/**
 * Voluntarily small for development/test
 * Unit: code unit/UChar, so minimum is 2 not 1! (don't take care of trailing \0)
 **/
// # define IN_BUFFER_SIZE 2
# define IN_BUFFER_SIZE 40
#else
# define IN_BUFFER_SIZE 1024
#endif
/**
 * 2*IN_BUFFER_SIZE because each char may be converted into a surrogate pair
 * (trailing \0 not counted)
 **/
#define OUT_BUFFER_SIZE 2*IN_BUFFER_SIZE

enum {
    UTR_EXIT_SUCCESS = 0,
    UTR_EXIT_FAILURE,
    UTR_EXIT_USAGE
};

enum {
    NONE,
    CHARACTER,       // a single code point, idea would be to optimize against STRING (more than one code point)
    STRING,
    SET,
    FILTER_FUNCTION,
    SIMPLE_FUNCTION, // simple translation (case mapping)
    GLOBAL_FUNCTION, // full translation (case mapping)
    COUNT
} /* set_type_t*/;

/*
+----------+----------+----+------+
| set1     | set2     | -c | note |
+----------+----------+----+------+
| function | none     | ko | ok (apply this translation function) => special case without -d, others are forbidden
+----------+----------+----+-------
| char     | char     | ok | strtr
| char     | string   | -- | forbidden
| char     | class    | -- | impossible
| char     | function | ok | ok => no sense ?
+----------+----------+----+-------
| string   | char     | ok | strtr
| string   | string   | ko | strtr
| string   | class    | -- | impossible
| string   | function | ok | ok => no sense ?
+----------+----------+----+-------
| set      | char     | ok | ok
| set      | string   | -- | forbidden
| set      | class    | -- | impossible
| set      | function | ok | ok => no sense ?
+----------+----------+----+-------
| function | char     | ok | ok
| function | string   | -- | forbidden
| function | class    | -- | impossible
| function | function | ok | ok => no sense ?
+----------+----------+----+-------

Note n°1:
For u_strToTitle, as translation function, may be really difficult (impossible?) to apply while reading input by multiple blocks.
Can we keep and reuse UBreakIterator state of the previous block?

Note n°2:
Don't forget that full case mapping can change the number of code points and/or code units of a string.
An output buffer 4 times larger than input buffer may be more appropriate?

Note n°3:
U+03A3 (Lu) // U+03C3/U+03C3 (Ll) may not work, depending on how(where) read is splitted


char: UChar32
string: UChar32 *
class: UChar *
function: - (stay with char *)
*/

typedef UBool (*filter_func_t)(UChar32);
typedef UChar32 (*simple_translate_func_t)(UChar32);

typedef struct {
    const char *name;
    filter_func_t func;
} filter_func_decl_t;

typedef struct {
    const char *name;
    UCaseType ct;
} case_map_element_t;

/* ========== global variables ========== */

UString *ustr = NULL;
UBreakIterator *ubrk = NULL;
UBool possible_untermminated_word = FALSE;

filter_func_decl_t filter_functions[] = {
    {"isupper",         u_isupper},
    {"islower",         u_islower},
    {"istitle",         u_istitle},
    {"isdigit",         u_isdigit},
    {"isalpha",         u_isalpha},
    {"isalnum",         u_isalnum},
    {"isxdigit",        u_isxdigit},
    {"ispunct",         u_ispunct},
    {"isgraph",         u_isgraph},
    {"isblank",         u_isblank},
    {"isdefined",       u_isdefined},
    {"isspace",         u_isspace},
    {"isJavaSpaceChar", u_isJavaSpaceChar},
    {"isWhitespace",    u_isWhitespace},
    {"iscntrl",         u_iscntrl},
    {"isISOControl",    u_isISOControl},
    {"isprint",         u_isprint},
    {"isbase",          u_isbase},
    {NULL,              NULL}
};

case_map_element_t case_map[] = {
    {"toupper", UCASE_UPPER},
    {"tolower", UCASE_LOWER},
    {"totitle", UCASE_TITLE},
    {NULL,      UCASE_NONE}
};

static const simple_translate_func_t simple_case_mapping[UCASE_COUNT] = {
    NULL,      // UCASE_NONE
    NULL,      // UCASE_FOLD
    u_tolower,
    u_toupper,
    u_totitle
};

/* ========== getopt stuff ========== */

/*enum {
    INPUT_OPT = CHAR_MAX + 1,
    READER_OPT
};*/

static char optstr[] = "Ccdst";

static struct option long_options[] =
{
    // only apply to stdin (not string from argv, always converted from system encoding)
    /*{"input",           required_argument, NULL, INPUT_OPT},
    {"reader",          required_argument, NULL, READER_OPT},*/
    GETOPT_COMMON_OPTIONS,
    {"complement",      no_argument,       NULL, 'c'},
    {"delete",          no_argument,       NULL, 'd'},
    {"squeeze-repeats", no_argument,       NULL, 's'},
    {"truncate",        no_argument,       NULL, 't'},
    {"version",         no_argument,       NULL, 'v'},
    {NULL,              no_argument,       NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [-%s] SET1 [SET2] [STRING]\n",
        __progname,
        optstr
    );
    exit(UTR_EXIT_USAGE);
}

/* ========== set helpers ========== */

USet *create_set_from_argv(const char *cpattern, UBool negate, error_t **error)
{
    USet *uset;
    UChar *upattern;
    UErrorCode status;
    int32_t upattern_length;

    status = U_ZERO_ERROR;
    if (NULL == (upattern = local_to_uchar(cpattern, &upattern_length, error))) {
        return NULL;
    }
    uset = uset_openPattern(upattern, upattern_length, &status);
    if (U_FAILURE(status)) {
        free(upattern);
        icu_error_set(error, FATAL, status, "uset_openPattern");
        return NULL;
    }
    free(upattern);
    if (negate) {
        uset_complement(uset);
    }
    uset_freeze(uset);

    return uset;
}

USet *create_set_from_string32(const UChar32 *string32, int32_t string32_length, UBool negate, error_t **UNUSED(error)) {
    USet *uset;
    int i;

    uset = uset_openEmpty();
    for (i = 0; i < string32_length; i++) {
        uset_add(uset, string32[i]);
    }
    if (negate) {
        uset_complement(uset);
    }
    uset_freeze(uset);

    return uset;
}

/* ========== replacement helpers ========== */

// use an hashtable for performance ?
void trtr(
    UChar32 *from, int32_t from_length,
    UChar32 *to, int32_t to_length,
    UChar *in, int32_t in_length,
    UChar *out, int32_t *out_length, int32_t out_size
) {
    int i, f;
    UChar32 ci;
    UBool isError;

    isError = FALSE; // unused
    for (i = 0; i < in_length; ) {
        U16_NEXT(in, i, in_length, ci);
        for (f = 0; f < from_length; f++) {
            if (from[f] == ci) {
                if (NULL == to || 0 == *to || 0 == to_length) { // dFlag on
                    goto endinnerloop;
                } else if (1 == to_length) { // dFlag off, to_length == 1 (to is a single code point)
                    U16_APPEND(out, *out_length, out_size, to[0], isError);
                    goto endinnerloop;
                } else { // dFlag off, to_length == from_length
                    U16_APPEND(out, *out_length, out_size, to[f], isError);
                    goto endinnerloop;
                }
            }
        }
        U16_APPEND(out, *out_length, out_size, ci, isError);

endinnerloop:
        ;
    }
}

void cptr(
    UChar32 from,
    UChar32 to,
    UChar *in, int32_t in_length,
    UChar *out, int32_t *out_length, int32_t out_size,
    UBool negate
) {
    int i;
    UChar32 ci;
    UBool isError;

    isError = FALSE; // unused
    for (i = 0; i < in_length; ) {
        U16_NEXT(in, i, in_length, ci);
        if (((from == ci) ^ negate)) {
            if (-1 != to) {
                U16_APPEND(out, *out_length, out_size, to, isError);
            }
            continue;
        }
        U16_APPEND(out, *out_length, out_size, ci, isError);
    }
}

/* ========== main ========== */

static void exit_cb(void)
{
    if (NULL != ustr) {
        ustring_destroy(ustr);
    }
    if (NULL != ubrk) {
        ubrk_close(ubrk);
    }
}

int main(int argc, char **argv)
{
    int c;
    USet *uset;
    UBool match;
    UChar32 i32;
    error_t *error;
    reader_t reader;
    UChar *in;
    UChar *from, *to;
    UErrorCode status;
    UChar32 *from32, *to32;
    filter_func_t filter_func;
    int set1_type, set2_type;
    UBool set2_expected, dFlag, cFlag, isError;
    UChar realin[IN_BUFFER_SIZE + 1 + 3]; // + 1 for \0, + 3 for fake characters
    UChar out[OUT_BUFFER_SIZE + 1];
    int32_t in_length, out_length;
    int32_t from_length, to_length;
    UCaseType set1_case_type, set2_case_type;

    uset = NULL;
    error = NULL;
    in = realin + 1;
    from = to = NULL;
    filter_func = NULL;
    from32 = to32 = NULL;
    status = U_ZERO_ERROR;
    set1_type = set2_type = NONE;
    exit_failure_value = UTR_EXIT_FAILURE;
    set1_case_type = set2_case_type = UCASE_NONE;
    isError = set2_expected = cFlag = dFlag = FALSE;

    realin[0] = 0x0020;
    // these following 'z', permit us to known if the word may be splitted or not
    realin[IN_BUFFER_SIZE + 1] = 0x007A; // z (test this offset as boundary)
    realin[IN_BUFFER_SIZE + 2] = 0x007A; // z (without it, boundary test were always true because we are at the end of the string)
    realin[IN_BUFFER_SIZE + 3] = 0; // just in case and debugging

    if (0 != atexit(exit_cb)) {
        fputs("can't register atexit() callback", stderr);
        return UTR_EXIT_FAILURE;
    }

    reader_init(&reader, DEFAULT_READER_NAME);

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'C':
            case 'c':
                cFlag = TRUE;
                break;
            case 'd':
                dFlag = TRUE;
                break;
            case 's':
                // TODO
                break;
            case 't':
                // NOP, ignored, doesn't apply to Unicode
                break;
            case 'v':
                fprintf(stderr, "utr version %u.%u\n", UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
            /*case READER_OPT:
                if (!reader_set_imp_by_name(&reader, optarg)) {
                    fprintf(stderr, "Unknown reader\n");
                    return UTR_EXIT_USAGE;
                }
                break;
            case INPUT_OPT:
                reader_set_default_encoding(&reader, optarg);
                break;*/
            default:
                if (!util_opt_parse(c, optarg, &reader)) {
                    usage();
                }
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (dFlag) {
        switch (argc) {
            case 1:
                // set1 (argv[0]) + stdin
                reader_open_stdin(&reader, &error);
                break;
            case 2:
                // set1 (argv[0]) + string (argv[1])
                reader_open_string(&reader, &error, argv[1]);
                break;
            default:
                usage();
                break;
        }
    } else {
        switch (argc) {
            case 1:
                // set1 (argv[0]) + stdin
                reader_open_stdin(&reader, &error);
                break;
            case 2:
                // set1 (argv[0]) as FUNC + string (argv[1])
                // OR
                // set1 (argv[0]) + set2 (argv[1]) + stdin
                if (!strncmp("fn:", argv[0], STR_LEN("fn:"))) {
                    reader_open_string(&reader,  &error, argv[1]);
                } else {
                    set2_expected = TRUE;
                    reader_open_stdin(&reader, &error);
                }
                break;
            case 3:
                // set1 (argv[0]) + set2 (argv[1]) + string (argv[2])
                set2_expected = TRUE;
                reader_open_string(&reader,  &error, argv[2]);
                break;
            default:
                usage();
                break;
        }
        if (set2_expected) {
            if (!strncmp("fn:", argv[1], STR_LEN("fn:"))) {
                case_map_element_t *c;

                for (c = case_map; NULL != c->name; c++) {
                    if (!strcmp(c->name, argv[1] + STR_LEN("fn:"))) {
                        set2_type = SIMPLE_FUNCTION;
                        set2_case_type = c->ct;
                        break;
                    }
                }
                if (UCASE_NONE == set2_case_type) {
                    fprintf(stderr, "Unknown translate function '%s'\n", argv[1] + STR_LEN("fn:"));
                    return UTR_EXIT_USAGE;
                }
            } else {
                if (NULL == (to32 = local_to_uchar32(argv[1], &to_length, &error))) {
                    print_error(error);
                    return UTR_EXIT_FAILURE;
                }
                if (1 == to_length) {
                    set2_type = CHARACTER;
                } else {
                    set2_type = STRING;
                }
            }
        }
    }
    if (!strncmp("fn:", argv[0], STR_LEN("fn:"))) {
        if (NONE == set2_type && !dFlag) {
            case_map_element_t *c;

            for (c = case_map; NULL != c->name; c++) {
                if (!strcmp(c->name, argv[0] + STR_LEN("fn:"))) {
                    set1_type = GLOBAL_FUNCTION;
                    set1_case_type = c->ct;
                    break;
                }
            }
            if (UCASE_NONE == set1_case_type) {
                fprintf(stderr, "Unknown translate function '%s'\n", argv[0] + STR_LEN("fn:"));
                return UTR_EXIT_USAGE;
            }
        } else {
            filter_func_decl_t *f;

            for (f = filter_functions; NULL != f->name; f++) {
                if (!strcmp(f->name, argv[0] + STR_LEN("fn:"))) {
                    set1_type = FILTER_FUNCTION;
                    filter_func = f->func;
                    break;
                }
            }
            if (NULL == filter_func) {
                fprintf(stderr, "Unknown filter function '%s'\n", argv[0] + STR_LEN("fn:"));
                return UTR_EXIT_USAGE;
            }
        }
    } else if ('[' == *argv[0]) {
        set1_type = SET;
        if (NULL == (uset = create_set_from_argv(argv[0], cFlag, &error))) {
            print_error(error);
        }
    } else {
        if (NULL == (from32 = local_to_uchar32(argv[0], &from_length, &error))) {
            print_error(error);
        }
        if (1 == from_length) {
            set1_type = CHARACTER;
        } else {
            if (cFlag) { // readjust
                if (NULL == (uset = create_set_from_string32(from32, from_length, cFlag, &error))) {
                    print_error(error);
                }
                set1_type = SET;
            } else {
                set1_type = STRING;
            }
        }
    }

    if (STRING == set2_type) {
        if (STRING != set1_type) {
            fprintf(stderr, "Using a string as a set have only sense if the first set is defined as a string too\n");
            return UTR_EXIT_FAILURE;
        } else {
            if (cFlag) {
                fprintf(stderr, "Complement option cannot be applied when the both sets are defined as strings\n");
                return UTR_EXIT_FAILURE;
            }
            if (from_length != to_length) {
                fprintf(stderr, "Number of code points differs between sets\n");
                return UTR_EXIT_FAILURE;
            }
        }
    }

    if (GLOBAL_FUNCTION == set1_type /*&& set2_type == NONE*/) {
        ustr = ustring_new();
        if (set1_case_type == UCASE_TITLE) {
            ubrk = ubrk_open(UBRK_WORD, NULL, NULL, 0, &status);
            if (U_FAILURE(status)) {
                icu_msg(FATAL, status, "ubrk_open");
            }
        }
    }

    while (!reader_eof(&reader)) {
        out_length = 0;
        if (-1 == (in_length = reader_readuchars(&reader, &error, in, IN_BUFFER_SIZE))) {
            print_error(error);
        }
// debug("in_length = %d", in_length);
        // don't append a nul character: work with lengths (needed for correct titlecasing)
        if (SET == set1_type || FILTER_FUNCTION == set1_type) {
            int i;

            for (i = 0; i < in_length; ) {
                U16_NEXT(in, i, in_length, i32);
                if (SET == set1_type) {
                    match = uset_contains(uset, i32);
                } else if (FILTER_FUNCTION == set1_type) {
                    match = filter_func(i32) ^ cFlag;
                } /*else { BUG }*/
                if (match) {
                    if (dFlag) {
                        continue;
                    } else if (CHARACTER == set2_type) {
                        i32 = *to32;
                    } else if (SIMPLE_FUNCTION == set2_type) {
                        i32 = simple_case_mapping[set2_case_type](i32);
                    }
                    U16_APPEND(out, out_length, OUT_BUFFER_SIZE, i32, isError);
                } else {
                    U16_APPEND(out, out_length, OUT_BUFFER_SIZE, i32, isError);
                }
            }
        } else if (CHARACTER == set1_type && CHARACTER == set2_type) {
            cptr(*from32, dFlag ? -1 : *to32, in, in_length, out, &out_length, OUT_BUFFER_SIZE, cFlag);
        } else if (SIMPLE_FUNCTION == set2_type) {
            if (CHARACTER == set1_type) {
                to32 = mem_new(*to32); // we don't use \0 in fact
                to32[0] = from32[0];
                to_length = 1;
            } else if (STRING == set1_type) {
                int i;

                to32 = mem_new_n(*to32, from_length); // we don't use \0 in fact, so forget "+ 1"
                to_length = from_length;
                for (i = 0; i < from_length; i++) {
                    to32[i] = simple_case_mapping[set2_case_type](from32[i]);
                }
            }
            trtr(from32, from_length, to32, to_length, in, in_length, out, &out_length, OUT_BUFFER_SIZE);
        } else if (GLOBAL_FUNCTION == set1_type /*&& set2_type == NONE*/) {
            // out and out_length unused: static buffer unappropriate for this case
            if (UCASE_TITLE == set1_case_type) {
                if (possible_untermminated_word) {
                    realin[0] = 0x0041; // prepend an uppercase letter (A): assume a splitted word doesn't become titlecased where it shouldn't
                } else {
                    realin[0] = 0x0020; // prepend a space: so we are not interfering in titlecasing
                }
                if (!ustring_fullcase(ustr, realin, in_length + 3, set1_case_type, ubrk, &error)) {
                    print_error(error);
                }
                u_file_write(ustr->ptr + 1, ustr->len - 3, ustdout);
                possible_untermminated_word = (in_length == IN_BUFFER_SIZE && !ubrk_isBoundary(ubrk, in_length + 1));
            } else {
                if (!ustring_fullcase(ustr, in, in_length, set1_case_type, ubrk, &error)) {
                    print_error(error);
                }
                u_file_write(ustr->ptr, ustr->len, ustdout);
            }
            continue;
        } else {
            trtr(from32, from_length, to32, to_length, in, in_length, out, &out_length, OUT_BUFFER_SIZE);
        }
        out[out_length] = 0;
        u_file_write(out, out_length, ustdout);
    }
    reader_close(&reader);

    if (NULL != from) {
        free(from);
    }
    if (NULL != to) {
        free(to);
    }
    if (NULL != from32) {
        free(from);
    }
    if (NULL != to32) {
        free(to);
    }
    if (NULL != uset) {
        uset_close(uset);
    }

    return UTR_EXIT_SUCCESS;
}
