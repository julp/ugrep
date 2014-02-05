#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/ucol.h>

#include "common.h"
#include "parsenum.h"
#include "struct/rbtree.h"


enum {
    USORT_EXIT_SUCCESS = 0,
    USORT_EXIT_FAILURE,
    USORT_EXIT_USAGE
};

typedef struct {
    int32_t start_field;
    int32_t end_field;
    int32_t start_offset;
    int32_t end_offset;
    uint64_t options; // bdfgiMhnRrV
} UsortKey;

#define USORT_OPT_IGNORE_LEAD_BLANKS (1 << 0)
#define USORT_OPT_DICTIONNARY_ORDER  (1 << 1)
#define USORT_OPT_IGNORE_CASE        (1 << 2)
#define USORT_OPT_IGNORE_NON_PRINT   (1 << 3)

#define USORT_OPT_GENERAL_NUM_SORT   (1 << 4)
#define USORT_OPT_MONTH_SORT         (1 << 5)
#define USORT_OPT_HUMAN_SORT         (1 << 6)
#define USORT_OPT_NUM_SORT           (1 << 7)
#define USORT_OPT_RANDOM_SORT        (1 << 8)
#define USORT_OPT_REVERSE_SORT       (1 << 9)
#define USORT_OPT_VERSION_SORT       (1 << 10)

/* ========== global variables ========== */

static const UChar DEFAULT_SEPARATOR[] = { U_HT, 0 };

static RBTree *tree = NULL;
static UString *ustr = NULL;
static UCollator *ucol = NULL;
static UString *separator = NULL;

static UBool bFlag = FALSE;
static UBool rFlag = FALSE;
static UBool uFlag = FALSE;

static uint64_t global_options = 0;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC,
    MIN_OPT,
    MAX_OPT,
    SORT_OPT,
    VERSION_OPT
};

enum {
    MIN_ONLY,
    MAX_ONLY,
    ALL
};

static char optstr[] = "bfk:nru";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"binary-files",          required_argument, NULL, BINARY_OPT},
    {"max",                   no_argument,       NULL, MAX_OPT},
    {"min",                   no_argument,       NULL, MIN_OPT},
    {"sort",                  required_argument, NULL, SORT_OPT},
    {"version",               no_argument,       NULL, VERSION_OPT},
    {"ignore-leading-blanks", no_argument,       NULL, 'b'},
    {"ignore-case",           no_argument,       NULL, 'f'},
    {"key",                   required_argument, NULL, 'k'},
    {"numeric-sort",          no_argument,       NULL, 'n'},
    {"reverse",               no_argument,       NULL, 'r'},
    {"unique",                no_argument,       NULL, 'u'},
    {NULL,                    no_argument,       NULL, 0}
};

struct x {
    const char *name;
    int short_opt_val;
    uint64_t flag_value;
} static sortnames[] = {
    {"general-numeric", 'g', USORT_OPT_GENERAL_NUM_SORT},
    {"human-numeric",   'h', USORT_OPT_HUMAN_SORT},
    {"month",           'M', USORT_OPT_MONTH_SORT},
    {"numeric",         'n', USORT_OPT_NUM_SORT},
    {"random",          'R', USORT_OPT_RANDOM_SORT},
    {"version",         'V', USORT_OPT_VERSION_SORT}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [-%s] [file ...]\n",
        __progname,
        optstr
    );
    exit(USORT_EXIT_USAGE);
}

/* ========== helpers ========== */

static void usort_print(const void *k, void *v)
{
    int i, count;
    UString *ustr;

    ustr = (UString *) k;
    if (NULL == v) {
        count = 1;
    } else {
        count = *((int *) v);
    }
    for (i = 0; i < count; i++) {
        u_fputs(ustr->ptr, ustdout);
    }
}

#if 0
static void usort_toustring(UString *ustr, const void *key, void *value)
{
    int count;
    UString *kstr;

    count = 1;
    if (NULL != value) {
        count =  *(int *) value;
    }
    kstr = (UString *) key;
    ustring_sprintf(ustr, ">%.*S< (%d) => %d", kstr->len, kstr->ptr, kstr->len, count);
}
#endif

static int parse_option(int opt, uint64_t *flags)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(sortnames); i++) {
        if (sortnames[i].short_opt_val == opt) {
            *flags |= sortnames[i].flag_value;
            return TRUE;
        }
    }

    return FALSE;
}

// \d+(.\d+)?[bdfgiMhnRrV]*(,\d+(.\d+)?[bdfgiMhnRrV]*)?
static UsortKey *parse_key(const char *string, error_t **error)
{
    int32_t min;
    char *endptr;
    const char *p;
    ParseNumError err;
    UsortKey key = {0}, *ret;

    min = 1;
    p = string;
    if (
        PARSE_NUM_NO_ERR != (err = parse_int32_t(p, &endptr, 10, &min, NULL, &key.start_field)) // ^\d+$
        && (PARSE_NUM_ERR_NON_DIGIT_FOUND != err)                                               // ^\d+
    ) {
        error_set(error, FATAL, "%s:\n%s\n%*c", "TODO", string, endptr - string + 1, '^');
        return NULL;
    }
    if ('.' == *endptr) {
        p = endptr + 1;
        if (
            PARSE_NUM_NO_ERR != (err = parse_int32_t(p, &endptr, 10, &min, NULL, &key.start_offset)) // ^\d+\.\d+$
            && PARSE_NUM_ERR_NON_DIGIT_FOUND != err                                                  // ^\d+\.\d+.*
        ) {
            error_set(error, FATAL, "%s:\n%s\n%*c", "TODO", string, endptr - string + 1, '^');
            return NULL;
        }
    }
    p = endptr;
    while ('\0' != *p && ',' != *p) { // check .* is in fact [bdfgiMhnRrV]*
        if (!parse_option(*p, &key.options)) {
            error_set(error, FATAL, "invalid option '%c'", *p);
            return NULL;
        }
        ++p;
    }
    if (',' == *p) {
        if (
            PARSE_NUM_NO_ERR != (err = parse_int32_t(++p, &endptr, 10, &min, NULL, &key.end_field)) // ,\d+$
            && (PARSE_NUM_ERR_NON_DIGIT_FOUND != err && '.' != *endptr)                             // ,\d+\.
        ) {
            error_set(error, FATAL, "%s:\n%s\n%*c", "TODO", string, endptr - string + 1, '^');
            return NULL;
        }
        p = endptr;
        if ('.' == *endptr) {
            if (
                PARSE_NUM_NO_ERR != (err = parse_int32_t(++p, &endptr, 10, &min, NULL, &key.end_offset)) // ,\d+\.\d+$
                && PARSE_NUM_ERR_NON_DIGIT_FOUND != err                                                  // ,\d+\.\d+.*$
            ) {
                error_set(error, FATAL, "%s:\n%s\n%*c", "TODO", string, endptr - string + 1, '^');
                return NULL;
            }
        }
        p = endptr;
        while ('\0' != *p) { // check .* is in fact [bdfgiMhnRrV]*
            if (!parse_option(*p, &key.options)) {
                error_set(error, FATAL, "invalid option '%c'", *p);
                return NULL;
            }
            ++p;
        }
    }
    if ('\0' != *p) {
        error_set(error, FATAL, "extra characters found:\n%s\n%*c", string, p - string + 1, '^');
        return NULL;
    }

    ret = mem_new(*ret);
    memcpy(ret, &key, sizeof(*ret));

    return ret;
}

// echo -en "1\n10\n12\n100\n101\n1" | ./usort
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
            if (bFlag) {
                ustring_ltrim(ustr);
            }
            if (uFlag) {
                rbtree_insert(tree, ustr, NULL, RBTREE_INSERT_ON_DUP_KEY_PRESERVE, NULL);
            } else {
                int v;
                int *p;

                v = 1;
                if (!rbtree_insert(tree, ustr, (void *) &v, RBTREE_INSERT_ON_DUP_KEY_FETCH, (void **) &p)) {
                    (*p)++;
                }
            }
        }
    }
    reader_close(reader);
    if (NULL != error) {
        print_error(error);
        return 1;
    }

    return 0;
}

/* ========== main ========== */

int main(int argc, char **argv)
{
    int wanted;
    int c, ret;
    error_t *error;
    reader_t *reader;
    UErrorCode status;

    ret = 0;
    wanted = ALL;
    error = NULL;
    env_init(USORT_EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);

    status = U_ZERO_ERROR;
    ucol = ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(&error, FATAL, status, "ucol_open");
        print_error(error);
    }

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'b':
                bFlag = TRUE;
                global_options |= USORT_OPT_IGNORE_LEAD_BLANKS;
                break;
            case 'f':
                ucol_setStrength(ucol, UCOL_SECONDARY);
                global_options |= USORT_OPT_IGNORE_CASE;
                break;
            case 'k':
                if (NULL == parse_key(optarg, &error)) {
                    print_error(error);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                ucol_setAttribute(ucol, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
                if (U_FAILURE(status)) {
                    //
                }
                global_options |= USORT_OPT_NUM_SORT;
                break;
            case 'r':
                rFlag = TRUE;
                global_options |= USORT_OPT_REVERSE_SORT;
                break;
            case 'u':
                uFlag = TRUE;
//                 global_options |= ?;
                break;
            case VERSION_OPT:
                fprintf(stderr, "BSD usort version %u.%u\n" COPYRIGHT, UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                return EXIT_SUCCESS;
            case MIN_OPT:
                wanted = MIN_ONLY;
                break;
            case MAX_OPT:
                wanted = MAX_ONLY;
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

//     if (ALL == wanted) {
        tree = rbtree_collated_new(ucol, rFlag, (dup_t) ustring_dup, uFlag ? NODUP : SIZE_TO_DUP_T(sizeof(int)), (func_dtor_t) ustring_destroy, uFlag ? NULL : free);
        env_register_resource(tree, (func_dtor_t) rbtree_destroy);
//     }
    ustr = ustring_new();
    env_register_resource(ustr, (func_dtor_t) ustring_destroy);

    if (0 == argc) {
        ret |= procfile(reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            // for memory consumption, with wanted equal to MIN_ONLY/MAX_ONLY
            // clear btree by keeping only min/max (which becomes root)
            // (min/max heap)
            ret |= procfile(reader, *argv);
        }
    }

    if (!rbtree_empty(tree)) {
        switch (wanted) {
            case MIN_ONLY:
            {
                // TODO: for MIN or MAX, use only 2 ustring instead of btree
                // (one to keep the min/max found ; the other to read the next element)
                void *k;

                rbtree_min(tree, &k, NULL);
                usort_print(k, NULL);
                break;
            }
            case MAX_ONLY:
            {
                void *k;

                rbtree_max(tree, &k, NULL);
                usort_print(k, NULL);
                break;
            }
            default:
            {
                rbtree_traverse(tree, IN_ORDER, usort_print);
            }
        }
    }

    return (0 == ret ? USORT_EXIT_SUCCESS : USORT_EXIT_FAILURE);
}
