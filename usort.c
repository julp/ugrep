#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/ucol.h>

#include "common.h"
#include "rbtree.h"


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
    BINARY_OPT = CHAR_MAX + 1,
    INPUT_OPT,
    READER_OPT,
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
    {"binary-files",          required_argument, NULL, BINARY_OPT},
    {"input",                 required_argument, NULL, INPUT_OPT},
    {"reader" ,               required_argument, NULL, READER_OPT},
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

static int usort_cmp(const void *k1, const void *k2)
{
    UString *s1, *s2;

    s1 = (UString *) k1;
    s2 = (UString *) k2;

    return ucol_strcoll(ucol, s1->ptr, s1->len, s2->ptr, s2->len);
}

static int usort_cmp_r(const void *k1, const void *k2)
{
    UString *s1, *s2;

    s1 = (UString *) k1;
    s2 = (UString *) k2;

    return ucol_strcoll(ucol, s2->ptr, s2->len, s1->ptr, s1->len);
}

static const func_cmp_t usort_cmp_func[] = { usort_cmp, usort_cmp_r };

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
                rbtree_insert(tree, ustr, NULL);
            } else {
                int *p;
                int side;
                void *res;
                RBTreeNode *parent;

                if (0 == rbtree_lookup_node(tree, ustr, &parent, &side, &res)) {
                    p = mem_new(*p);
                    *p = 1;
                    rbtree_insert_node(tree, (RBTreeNode*) res, p, parent, side);
                } else {
                    p = (int *) res;
                    *p = *p + 1;
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
    reader_init(&reader, "mmap");
    exit_failure_value = USORT_EXIT_FAILURE;
    //ustdio_init();

    status = U_ZERO_ERROR;
    ucol = ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(&error, FATAL, status, "ucol_open");
        print_error(error);
    }

/*
Insensible aux accents, sensible à la casse : attribut Collator::STRENGTH à Collator::PRIMARY et Collator::CASE_LEVEL à Collator::ON
Insensible aux accents et à la casse : Collator::STRENGTH à Collator::PRIMARY et Collator::CASE_LEVEL à Collator::OFF
*/
    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'b':
                bFlag = TRUE;
                break;
            case 'f':
                ucol_setStrength(ucol, UCOL_PRIMARY);
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
                fprintf(stderr, "usort version %u.%u\n", UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
            case MIN_OPT:
                wanted = MIN_ONLY;
                break;
            case MAX_OPT:
                wanted = MAX_ONLY;
                break;
            case READER_OPT:
                if (!reader_set_imp_by_name(&reader, optarg)) {
                    fprintf(stderr, "Unknown reader\n");
                    return USORT_EXIT_USAGE;
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

//     if (ALL == wanted) {
        tree = rbtree_new(usort_cmp_func[!!rFlag], (func_dtor_t) ustring_destroy, uFlag ? NULL : free);
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
