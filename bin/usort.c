#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/ucol.h>

#include "common.h"
#include "struct/rbtree.h"


enum {
    USORT_EXIT_SUCCESS = 0,
    USORT_EXIT_FAILURE,
    USORT_EXIT_USAGE
};

/* ========== global variables ========== */

RBTree *tree = NULL;
UCollator *ucol = NULL;

UBool bFlag = FALSE;
UBool rFlag = FALSE;
UBool uFlag = FALSE;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC,
    MIN_OPT,
    MAX_OPT
};

enum {
    MIN_ONLY,
    MAX_ONLY,
    ALL
};

static char optstr[] = "bfnru";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"binary-files",          required_argument, NULL, BINARY_OPT},
    {"min",                   no_argument,       NULL, MIN_OPT},
    {"max",                   no_argument,       NULL, MAX_OPT},
    {"ignore-leading-blanks", no_argument,       NULL, 'b'},
    {"ignore-case",           no_argument,       NULL, 'f'},
    {"numeric-sort",          no_argument,       NULL, 'n'},
    {"reverse",               no_argument,       NULL, 'r'},
    {"unique",                no_argument,       NULL, 'u'},
    {NULL,                    no_argument,       NULL, 0}
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

// echo -en "1\n10\n12\n100\n101\n1" | ./usort
static int procfile(reader_t *reader, const char *filename)
{
    error_t *error;

    error = NULL;
    if (reader_open(reader, &error, filename)) {
        while (!reader_eof(reader)) {
            UString *ustr;

            ustr = ustring_new();
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            ustring_chomp(ustr);
            if (bFlag) {
                ustring_ltrim(ustr);
            }
            if (uFlag) {
                rbtree_insert(tree, ustr, NULL, 0, NULL);
            } else {
                int v;
                int *p;

                v = 1;
                if (!rbtree_insert(tree, ustr, (void *) &v, RBTREE_INSERT_ON_DUP_KEY_FETCH, (void **) &p)) {
                    (*p)++;
                }
            }
        }
        reader_close(reader);
    } else {
        print_error(error);
        return 1;
    }

    return 0;
}

/* ========== main ========== */

static void exit_cb(void)
{
    if (NULL != tree) {
        rbtree_destroy(tree);
    }
}

int main(int argc, char **argv)
{
    int wanted;
    int c, ret;
    error_t *error;
    reader_t reader;
    UErrorCode status;

    if (0 != atexit(exit_cb)) {
        fputs("can't register atexit() callback", stderr);
        return USORT_EXIT_FAILURE;
    }

    ret = 0;
    wanted = ALL;
    error = NULL;
    env_init();
    reader_init(&reader, DEFAULT_READER_NAME);
    exit_failure_value = USORT_EXIT_FAILURE;

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
                break;
            case 'f':
                ucol_setStrength(ucol, UCOL_SECONDARY);
                break;
            case 'n':
                ucol_setAttribute(ucol, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
                if (U_FAILURE(status)) {
                    //
                }
                break;
            case 'r':
                rFlag = TRUE;
                break;
            case 'u':
                uFlag = TRUE;
                break;
            case 'V':
                fprintf(stderr, "BSD usort version %u.%u\n" COPYRIGHT, UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
            case MIN_OPT:
                wanted = MIN_ONLY;
                break;
            case MAX_OPT:
                wanted = MAX_ONLY;
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

//     if (ALL == wanted) {
        tree = rbtree_collated_new(ucol, rFlag, NODUP, uFlag ? NODUP : SIZE_TO_DUP_T(sizeof(int)), (func_dtor_t) ustring_destroy, uFlag ? NULL : free);
//     }

    if (0 == argc) {
        ret |= procfile(&reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            // for memory consumption, with wanted equal to MIN_ONLY/MAX_ONLY
            // clear btree by keeping only min/max (which becomes root)
            // (min/max heap)
            ret |= procfile(&reader, *argv);
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
