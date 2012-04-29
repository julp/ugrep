#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WITH_FTS
# include <fts.h>
#endif /* WITH_FTS */
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include "common.h"


// =0: all input files were output successfully
// >0: an error occurred
enum {
    UCAT_EXIT_SUCCESS = 0,
    UCAT_EXIT_FAILURE,
    UCAT_EXIT_USAGE
};

/* ========== global variables ========== */

static UString *ustr = NULL;
static size_t lineno = 0;
static UBool EFlag = FALSE;
static UBool TFlag = FALSE;
static UBool bFlag = FALSE;
static UBool nFlag = FALSE;
static UBool sFlag = FALSE;
static UBool vFlag = FALSE;
static UBool file_print = FALSE; // -H/h
static int binbehave = BIN_FILE_SKIP;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC
};

static char optstr[] = "AEHTVbehnqstuv" FTS_COMMON_OPTIONS_STRING;

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
#ifdef WITH_FTS
    FTS_COMMON_OPTIONS,
#endif /* WITH_FTS */
    {"binary-files",     required_argument, NULL, BINARY_OPT}, // grep
    {"show-all",         no_argument,       NULL, 'A'},
    {"show-ends",        no_argument,       NULL, 'E'},
    {"with-filename",    no_argument,       NULL, 'H'}, // grep
    {"show-tabs",        no_argument,       NULL, 'T'},
    {"version",          no_argument,       NULL, 'V'},
    {"number-nonblank",  no_argument,       NULL, 'b'},
    {"no-filename",      no_argument,       NULL, 'h'}, // grep
    {"number",           no_argument,       NULL, 'n'},
    {"quiet",            no_argument,       NULL, 'q'}, // grep
    {"silent",           no_argument,       NULL, 'q'}, // grep
    {"squeeze-blank",    no_argument,       NULL, 's'},
    {"show-no-printing", no_argument,       NULL, 'v'},
    {NULL,               no_argument,       NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [-%s] [file ...]\n",
        __progname,
        optstr
    );
    exit(UCAT_EXIT_USAGE);
}

static int procfile(reader_t *reader, const char *filename, void *UNUSED(userdata))
{
    error_t *error;
    UBool numbered, count, prev_was_blank;

    error = NULL;
    count = TRUE;
    numbered = nFlag;
    prev_was_blank = FALSE;

    if (reader_open(reader, &error, filename)) {
        if (file_print) {
            lineno = 0;
            u_fprintf(ustdout, "%s:\n", filename);
        }
        /* !fd->binary || (fd->binary && BIN_FILE_BIN != binbehave) */
        while (NULL != reader_readline(reader, &error, ustr)) {
            ustring_chomp(ustr);
            if (bFlag || sFlag) {
                UBool blank;

                blank = ustring_empty(ustr);
                if (bFlag) {
                    numbered = count = !blank;
                }
                if (sFlag) {
                    if (prev_was_blank && (prev_was_blank = blank)) {
                        continue;
                    }
                    prev_was_blank = blank;
                }
            }
            if (count) {
                lineno++;
            }
            if (BIN_FILE_TEXT == binbehave || vFlag) {
                ustring_dump(ustr);
            }
            if (EFlag) {
                ustring_append_char(ustr, 0x0024);
            }
            if (numbered) {
                u_fprintf(ustdout, "%6d  ", lineno);
            }
            u_fputs(ustr->ptr, ustdout);
        }
        reader_close(reader);
    } else {
        print_error(error);
        return 1;
    }

    return 0;
}

#if 0
#ifdef WITH_FTS
static int procdir(reader_t *reader, char **dirname)
{
    int ret;
    FTS *fts;
    FTSENT *p;
    int ftsflags;

    ret = 0;
    /*ftsflags = 0;
    if (Hflag)
        ftsflags = FTS_COMFOLLOW;
    if (Pflag)
        ftsflags = FTS_PHYSICAL;
    if (Sflag)*/
        ftsflags = FTS_LOGICAL;
    ftsflags |= FTS_NOSTAT | FTS_NOCHDIR;

    if (NULL == (fts = fts_open(dirname, ftsflags, NULL))) {
        msg(FATAL, "can't fts_open %s: %s", *dirname, strerror(errno));
    }
    while (NULL != (p = fts_read(fts))) {
        switch (p->fts_info) {
            case FTS_DNR:
            case FTS_ERR:
                msg(WARN, "fts_read failed on %s: %s", p->fts_path, strerror(p->fts_errno));
                break;
            case FTS_D:
            case FTS_DP:
                break;
            default:
                ret |= procfile(reader, p->fts_path);
                break;
        }
    }
    fts_close(fts);

    return ret;
}
#endif /* WITH_FTS */
#endif

/* ========== main ========== */

int main(int argc, char **argv)
{
    int c, ret;
    reader_t *reader;

    ret = 0;
    env_init(UCAT_EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);

#if defined(HAVE_BZIP2) || defined(HAVE_ZLIB) || defined(DYNAMIC_READERS)
    switch (__progname[1]) {
# if defined(HAVE_BZIP2) || defined(DYNAMIC_READERS)
        case 'b':
            if ('z' == __progname[2]) {
                reader_set_imp_by_name(reader, "bzip2");
            }
            break;
# endif /* HAVE_BZIP2 || DYNAMIC_READERS */
# if defined(HAVE_ZLIB) || defined(DYNAMIC_READERS)
        case 'z':
            reader_set_imp_by_name(reader, "gzip");
            break;
# endif /* HAVE_ZLIB || DYNAMIC_READERS */
    }
#endif /* HAVE_BZIP2 || HAVE_ZLIB */

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'A':
                vFlag = TRUE;
                EFlag = TRUE;
                TFlag = TRUE;
                break;
            case 'E':
                EFlag = TRUE;
                break;
            case 'H':
                file_print = TRUE;
                break;
            case 'T':
                vFlag = TRUE; // TODO?
                TFlag = TRUE;
                break;
            case 'V':
                fprintf(stderr, "BSD ucat version %u.%u\n" COPYRIGHT, UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                return UCAT_EXIT_SUCCESS;
            case 'b':
                bFlag = TRUE;
                nFlag = FALSE;
                break;
            case 'e':
                vFlag = TRUE;
                EFlag = TRUE;
                break;
            case 'h':
                file_print = FALSE;
                break;
            case 'n':
                bFlag = FALSE;
                nFlag = TRUE;
                break;
            case 'q':
                env_set_verbosity(FATAL);
                break;
            case 's':
                sFlag = TRUE;
                break;
            case 't':
                vFlag = TRUE;
                TFlag = TRUE;
                break;
            case 'u': // POSIX
                // NOP, ignored
                break;
            case 'v':
                vFlag = TRUE;
                break;
            case BINARY_OPT:
                if (!strcmp("binary", optarg)) {
                    binbehave = BIN_FILE_BIN;
                } else if (!strcmp("text", optarg)) {
                    binbehave = BIN_FILE_TEXT;
                } else {
                    fprintf(stderr, "Unknown binary-files option\n");
                    return UCAT_EXIT_USAGE;
                }
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

    reader_set_binary_behavior(reader, binbehave);

    ustr = ustring_new();
    env_register_resource(ustr, (func_dtor_t) ustring_destroy);

    if (0 == argc) {
        ret |= procfile(reader, "-", NULL);
#ifdef WITH_FTS
    } else if (DIR_RECURSE == get_dirbehave()) {
        ret |= procdir(reader, argv, NULL, procfile);
#endif /* WITH_FTS */
    } else {
        for ( ; argc--; ++argv) {
#ifdef WITH_FTS
            if (!is_file_matching(*argv)) {
                continue;
            }
#endif /* WITH_FTS */
            ret |= procfile(reader, *argv, NULL);
        }
    }

    return (0 == ret ? UCAT_EXIT_SUCCESS : UCAT_EXIT_FAILURE);
}
