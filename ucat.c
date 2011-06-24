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


// =0: all input files were output successfully
// >0: an error occurred
enum {
    UCAT_EXIT_SUCCESS = 0,
    UCAT_EXIT_FAILURE,
    UCAT_EXIT_USAGE
};

/* ========== global variables ========== */

UString *ustr = NULL;
size_t lineno = 0;
UBool EFlag = FALSE;
UBool TFlag = FALSE;
UBool bFlag = FALSE;
UBool nFlag = FALSE;
UBool sFlag = FALSE;
UBool vFlag = FALSE;
UBool file_print = FALSE; // -H/h
int binbehave = BIN_FILE_SKIP;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = CHAR_MAX + 1,
    INPUT_OPT,
    READER_OPT
};

#ifndef WITHOUT_FTS
static char optstr[] = "AEHRTVbehnqrstuv";
#else
static char optstr[] = "AEHTVbehnqstuv";
#endif /* !WITHOUT_FTS */

static struct option long_options[] =
{
    {"binary-files",        required_argument, NULL, BINARY_OPT}, // grep
    {"input",               required_argument, NULL, INPUT_OPT}, // grep
    {"reader",              required_argument, NULL, READER_OPT}, // grep
    {"show-all",            no_argument,       NULL, 'A'},
    {"show-ends",           no_argument,       NULL, 'E'},
    {"with-filename",       no_argument,       NULL, 'H'}, // grep
#ifndef WITHOUT_FTS
    {"recursive",           no_argument,       NULL, 'R'}, // grep
#endif /* !WITHOUT_FTS */
    {"show-tabs",           no_argument,       NULL, 'T'},
    {"version",             no_argument,       NULL, 'V'},
    {"number-nonblank",     no_argument,       NULL, 'b'},
    {"no-filename",         no_argument,       NULL, 'h'}, // grep
    {"number",              no_argument,       NULL, 'n'},
    {"quiet",               no_argument,       NULL, 'q'}, // grep
    {"silent",              no_argument,       NULL, 'q'}, // grep
#ifndef WITHOUT_FTS
    {"recursive",           no_argument,       NULL, 'r'}, // grep
#endif /* !WITHOUT_FTS */
    {"squeeze-blank",       no_argument,       NULL, 's'},
    {"show-no-printing",    no_argument,       NULL, 'v'},
    {NULL,                  no_argument,       NULL, 0}
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

static int procfile(reader_t *reader, const char *filename)
{
    UBool numbered, count, prev_was_blank;
    error_t *error;

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
        while (!reader_eof(reader)) {
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
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

#ifndef WITHOUT_FTS
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
#endif /* !WITHOUT_FTS */

/* ========== main ========== */

static void exit_cb(void)
{
    if (NULL != ustr) {
        ustring_destroy(ustr);
    }
}

int main(int argc, char **argv)
{
    int c, ret;
    reader_t reader;
#ifndef WITHOUT_FTS
    UBool rFlag = FALSE;
#endif /* !WITHOUT_FTS */

    if (0 != atexit(exit_cb)) {
        fputs("can't register atexit() callback", stderr);
        return UCAT_EXIT_FAILURE;
    }

    ret = 0;
    reader_init(&reader, "mmap");
    exit_failure_value = UCAT_EXIT_FAILURE;
    //ustdio_init();

#if defined(HAVE_BZIP2) || defined(HAVE_ZLIB)
    switch (__progname[0]) {
# ifdef HAVE_BZIP2
        case 'b':
            if ('z' == __progname[1]) {
                reader_set_imp_by_name(&reader, "bzip2");
            }
            break;
# endif /* HAVE_BZIP2 */
# ifdef HAVE_ZLIB
        case 'z':
            reader_set_imp_by_name(&reader, "gzip");
            break;
# endif /* HAVE_ZLIB */
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
#ifndef WITHOUT_FTS
            case 'R':
                rFlag = TRUE;
                break;
#endif /* !WITHOUT_FTS */
            case 'T':
                vFlag = TRUE; // TODO?
                TFlag = TRUE;
                break;
            case 'V':
                fprintf(stderr, "ucat version %u.%u\n", UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
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
                verbosity = FATAL;
                break;
#ifndef WITHOUT_FTS
            case 'r':
                rFlag = TRUE;
                break;
#endif /* !WITHOUT_FTS */
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
            case READER_OPT:
                if (!reader_set_imp_by_name(&reader, optarg)) {
                    fprintf(stderr, "Unknown reader\n");
                    return UCAT_EXIT_USAGE;
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

    reader_set_binary_behavior(&reader, binbehave);

    ustr = ustring_new();

    if (0 == argc) {
        ret |= procfile(&reader, "-");
#ifndef WITHOUT_FTS
    } else if (rFlag) {
        ret |= procdir(&reader, argv);
#endif /* !WITHOUT_FTS */
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(&reader, *argv);
        }
    }

    return (0 == ret ? UCAT_EXIT_SUCCESS : UCAT_EXIT_FAILURE);
}
