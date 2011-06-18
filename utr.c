#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/uset.h>

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
# define IN_BUFFER_SIZE 2
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
    CHARACTER, // set2: character or function
    STRING,    // set2: string, function or character
    CLASS,     // set2: function
    FUNCTION,  // for set2 only
} /* set_type_t*/;

/*
set1 | set2 | note

char | char | strtr
char | string | forbidden
char | class | impossible
char | function | ok

string | char     | strtr
string | string   | strtr
string | class    | impossible
string | function | ok

class | char     | ok
class | string   | forbidden
class | class    | impossible
class | function | ok

function | char     | ok
function | string   | forbidden
function | class    | impossible
function | function | ok

char: UChar32
string: UChar32 *
class: UChar *
function: - (stay with char *)
*/

typedef UBool (*filter_func_t)(UChar32);
typedef UChar32 (*translate_func_t)(UChar32);

typedef struct {
    const char *name;
    filter_func_t func;
} filter_func_decl_t;

typedef struct {
    const char *name;
    translate_func_t func;
} translate_func_decl_t;

/* ========== global variables ========== */

filter_func_decl_t filter_functions[] = {
    {"isupper", u_isupper},
    {"islower", u_islower},
    {"istitle", u_istitle},
    {"isdigit", u_isdigit},
    {"isalpha", u_isalpha},
    {"isalnum", u_isalnum},
    {"isxdigit", u_isxdigit},
    {"ispunct", u_ispunct},
    {"isgraph", u_isgraph},
    {"isblank", u_isblank},
    {"isdefined", u_isdefined},
    {"isspace", u_isspace},
    {"isJavaSpaceChar", u_isJavaSpaceChar},
    {"isWhitespace", u_isWhitespace},
    {"iscntrl", u_iscntrl},
    {"isISOControl", u_isISOControl},
    {"isprint", u_isprint},
    {"isbase", u_isbase},
    {NULL, NULL}
};

translate_func_decl_t translate_functions[] = {
    {"toupper", u_toupper},
    {"tolower", u_tolower},
    {"totitle", u_totitle},
    {NULL, NULL}
};

/* ========== getopt stuff ========== */

enum {
    INPUT_OPT = CHAR_MAX + 1,
    READER_OPT
};

static char optstr[] = "Ccdst";

static struct option long_options[] =
{
    // only apply to stdin (not string from argv always converted from system encoding
    {"input",           required_argument, NULL, INPUT_OPT},
    {"reader",          required_argument, NULL, READER_OPT},
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
                } else if (1 == to_length) { // dFlag off, to_length == 1
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

int main(int argc, char **argv)
{
    int c;
    USet *uset;
    UBool match;
    UChar32 i32;
    error_t *error;
    reader_t reader;
    UChar *from, *to;
    UChar32 *from32, *to32;
    translate_func_t tr_func;
    filter_func_t filter_func;
    int set1_type, set2_type;
    UBool dFlag, cFlag, isError;
    UChar in[IN_BUFFER_SIZE + 1];
    UChar out[OUT_BUFFER_SIZE + 1];
    int32_t in_length, out_length;
    int32_t from_length, to_length;

    uset = NULL;
    error = NULL;
    tr_func = NULL;
    from = to = NULL;
    filter_func = NULL;
    from32 = to32 = NULL;
    isError = cFlag = dFlag = FALSE;
    exit_failure_value = UTR_EXIT_FAILURE;

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
            case READER_OPT:
                if (!reader_set_imp_by_name(&reader, optarg)) {
                    fprintf(stderr, "Unknown reader\n");
                    return UTR_EXIT_USAGE;
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

    if (dFlag) {
        switch (argc) {
            case 1:
                // set1 (argv[0]) + stdin
                reader_open_stdin(&reader, &error);
                break;
            case 2:
                // set1 (argv[0]) + string
                reader_open_string(&reader, &error, argv[1]);
                break;
            default:
                usage();
                break;
        }
    } else {
        switch (argc) {
            case 2:
                // set1 (argv[0]) + set2 (argv[1]) + stdin
                reader_open_stdin(&reader, &error);
                break;
            case 3:
                // set1 (argv[0]) + set2 (argv[1]) + string
                reader_open_string(&reader,  &error, argv[2]);
                break;
            default:
                usage();
                break;
        }
        if (!strncmp("fn:", argv[1], STR_LEN("fn:"))) {
            translate_func_decl_t *f;

            for (f = translate_functions; NULL != f->name; f++) {
                if (!strcmp(f->name, argv[1] + STR_LEN("fn:"))) {
                    set2_type = FUNCTION;
                    tr_func = f->func;
                    break;
                }
            }
            if (NULL == tr_func) {
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
    if (!strncmp("fn:", argv[0], STR_LEN("fn:"))) {
        filter_func_decl_t *f;

        for (f = filter_functions; NULL != f->name; f++) {
            if (!strcmp(f->name, argv[0] + STR_LEN("fn:"))) {
                set1_type = FUNCTION;
                filter_func = f->func;
                break;
            }
        }
        if (NULL == filter_func) {
            fprintf(stderr, "Unknown filter function '%s'\n", argv[0] + STR_LEN("fn:"));
            return UTR_EXIT_USAGE;
        }
    } else if ('[' == *argv[0]) {
        set1_type = CLASS;
        if (NULL == (uset = create_set_from_argv(argv[0], cFlag, &error))) {
            print_error(error);
            return UTR_EXIT_FAILURE;
        }
    } else {
        if (NULL == (from32 = local_to_uchar32(argv[0], &from_length, &error))) {
            print_error(error);
            return UTR_EXIT_FAILURE;
        }
        if (1 == from_length) {
            set1_type = CHARACTER;
        } else {
            set1_type = STRING;
        }
    }

    if (STRING == set2_type) {
        if (STRING != set1_type) {
            return UTR_EXIT_FAILURE;
        } else {
            if (from_length != to_length) {
                fprintf(stderr, "Number of code points differs between sets\n");
                return UTR_EXIT_FAILURE;
            }
        }
    }

    debug("isAlpha = %d", u_isalpha(0x0001D63C));

    while (!reader_eof(&reader)) {
        out_length = 0;
        if (-1 == (in_length = reader_readuchars(&reader, &error, in, IN_BUFFER_SIZE))) {
            print_error(error);
        }
        debug("in_length = %d", in_length);
        in[in_length] = 0;
        if (CLASS == set1_type || FUNCTION == set1_type) {
            int i;

            for (i = 0; i < in_length; ) {
                U16_NEXT(in, i, in_length, i32);
                if (CLASS == set1_type) {
                    match = uset_contains(uset, i32);
                } else if (FUNCTION == set1_type) {
                    match = filter_func(i32);
                } /*else { BUG }*/
                // TODO: consider cFlag
                if (match) {
                    if (dFlag) {
                        continue;
                    } else if (CHARACTER == set2_type) {
                        i32 = to32[0];
                    } else if (FUNCTION == set2_type) {
                        i32 = tr_func(i32);
                    }
                    U16_APPEND(out, out_length, OUT_BUFFER_SIZE, i32, isError);
                } else {
                    U16_APPEND(out, out_length, OUT_BUFFER_SIZE, i32, isError);
                }
            }
#if 0
        } else if (dFlag /*&& (STRING == set1_type || CHARACTER == set1_type)*/) {
            int i;

            for (i = 0; i < in_length; ) {
                U16_NEXT(in, i, in_length, i32);
                if (STRING == set1_type) {
                    match = ?;
                } else if (CHARACTER == set1_type) {
                    match = i32 == f32;
                } /*else { BUG }*/
                // TODO: consider cFlag
                if (!match) {
                    U16_APPEND(out, out_length, OUT_BUFFER_SIZE, i32, isError);
                }
            }
#endif
        } else if (FUNCTION == set2_type) {
            if (CHARACTER == set1_type) {
                to32 = mem_new(*to32); // we don't use \0 in fact
                to32[0] = from32[0];
                to_length = 1;
            } else if (STRING == set1_type) {
                int i;

                to32 = mem_new_n(*to32, from_length); // we don't use \0 in fact, so forget "+ 1"
                to_length = from_length;
                for (i = 0; i < from_length; i++) {
                    to32[i] = tr_func(from32[i]);
                }
            }
            trtr(from32, from_length, to32, to_length, in, in_length, out, &out_length, OUT_BUFFER_SIZE);
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
