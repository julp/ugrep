#include <limits.h>
#ifndef WITHOUT_FTS
# include <fts.h>
#endif /* !WITHOUT_FTS */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#define BINARY 1
#include "common.h"
#include "reader_decl.h"


/**
TODO:
* factoriser en général
* options communes en macro dans binary.h ?
***/


// =0: all input files were output successfully
// >0: an error occurred
enum {
    UCAT_EXIT_SUCCESS = 0,
    UCAT_EXIT_FAILURE,
    UCAT_EXIT_USAGE
};

/* ========== global variables ========== */

reader_t *available_readers[] = {
    &mm_reader,
    &stdio_reader,
#ifdef HAVE_ZLIB
    &gz_reader,
#endif /* HAVE_ZLIB */
#ifdef HAVE_BZIP2
    &bz2_reader,
#endif /* HAVE_BZIP2 */
    NULL
};

static reader_t *default_reader = NULL;

UString *ustr = NULL;
UBool EFlag = FALSE;
UBool nFlag = FALSE;
UBool file_print = FALSE;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = CHAR_MAX + 1,
    READER_OPT
};

#ifndef WITHOUT_FTS
static char optstr[] = "AEHRTVbehnqrstuv";
#else
static char optstr[] = "TODO";
#endif /* !WITHOUT_FTS */

static struct option long_options[] =
{
    {"binary-files",        required_argument, NULL, BINARY_OPT}, // grep
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
        "usage: %s TODO\n",
        __progname
    );
    exit(UCAT_EXIT_USAGE);
}

static int procfile(fd_t *fd, const char *filename)
{
    error_t *error;

    error = NULL;
    fd->reader = default_reader; // Restore default (stdin requires a switch on stdio)

    if (fd_open(&error, fd, filename)) {

        /* !fd->binary || (fd->binary && BIN_FILE_BIN != binbehave) */
        while (!fd_eof(fd)) {
            if (!fd_readline(&error, fd, ustr)) {
                print_error(error);
            }
            fd->lineno++;
            ustring_chomp(ustr);
            if (BIN_FILE_TEXT == binbehave) {
                ustring_dump(ustr);
            }
            if (EFlag) {
                ustring_append_char(ustr, 0x0024);
            }
            if (nFlag) {
                u_fprintf(ustdout, "%6d  ", fd->lineno);
            }
            u_fputs(ustr->ptr, ustdout);
        }
        fd_close(fd);
    } else {
        print_error(error);
    }

    return fd->matches;
}

#ifndef WITHOUT_FTS
static int procdir(fd_t *fd, char **dirname)
{
    FTS *fts;
    FTSENT *p;
    int matches;
    int ftsflags;

    matches = 0;
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
                /* matches += */ procfile(fd, p->fts_path);
                break;
        }
    }
    fts_close(fts);

    /*return matches;*/
    return 0;
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
    int c;
    fd_t fd = { 0, 0, 0, 0, 0, 0, 0, 0 };
#ifndef WITHOUT_FTS
    UBool rFlag = FALSE;
#endif /* !WITHOUT_FTS */
    error_t *error;

    error = NULL;

    if (0 != atexit(exit_cb)) {
        fputs("can't register atexit() callback", stderr);
        return UCAT_EXIT_FAILURE;
    }

    default_reader = &mm_reader;
    exit_failure_value = UCAT_EXIT_FAILURE;
    //ustdio_init();

#if defined(HAVE_BZIP2) || defined(HAVE_ZLIB)
    switch (__progname[0]) {
# ifdef HAVE_BZIP2
        case 'b':
            if ('z' == __progname[1]) {
                default_reader = &bz2_reader;
            }
            break;
# endif /* HAVE_BZIP2 */
# ifdef HAVE_ZLIB
        case 'z':
            default_reader = &gz_reader;
            break;
# endif /* HAVE_ZLIB */
    }
#endif /* HAVE_BZIP2 || HAVE_ZLIB */

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
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
            case 'V':
                fprintf(stderr, "ucat version %u.%u\n", UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
#ifndef WITHOUT_FTS
            case 'r':
                rFlag = TRUE;
                break;
#endif /* !WITHOUT_FTS */
            case 'n':
                nFlag = TRUE;
                break;
            case 'u': // POSIX
                // NOP, ignored
                break;
            case BINARY_OPT:
                if (!strcmp("binary", optarg)) {
                    binbehave = BIN_FILE_BIN;
                } else if (!strcmp("without-match", optarg)) {
                    binbehave = BIN_FILE_SKIP;
                } else if (!strcmp("text", optarg)) {
                    binbehave = BIN_FILE_TEXT;
                } else {
                    fprintf(stderr, "Unknown binary-files option\n");
                    return UCAT_EXIT_USAGE;
                }
                break;
            case READER_OPT:
                {
                    reader_t **r;

                    for (r = available_readers, default_reader = NULL; *r; r++) {
                        if (!strcmp((*r)->name, optarg)) {
                            default_reader = *r;
                            break;
                        }
                    }
                    if (NULL == default_reader) {
                        fprintf(stderr, "Unknown reader\n");
                        return UCAT_EXIT_USAGE;
                    }
                }
                break;
            default:
                usage();
                break;
        }
    }
    argc -= optind;
    argv += optind;

    ustr = ustring_new();

    if (0 == argc) {
        /* x = */ procfile(&fd, "-");
#ifndef WITHOUT_FTS
    } else if (rFlag) {
        /* x = */ procdir(&fd, argv);
#endif /* !WITHOUT_FTS */
    } else {
        for ( ; argc--; ++argv) {
            /* x = */ procfile(&fd, *argv);
        }
    }

    return UCAT_EXIT_SUCCESS;
}
