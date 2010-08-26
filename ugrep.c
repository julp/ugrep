#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include "ugrep.h"

#define MAX_ENC_REL_LEN 4096 // Maximum relevant length for encoding analyse (in bytes)
#define MAX_BIN_REL_LEN 4096 // Maximum relevant length for binary analyse

#define EXIT_USAGE 2

#define RED(str) "\x1b[1;31m" str "\x1b[0m"
#define GREEN(str) "\x1b[1;32m" str "\x1b[0m"

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

enum {
    PATTERN_LITERAL,
    PATTERN_REGEXP,
    PATTERN_AUTO
};

/* ========== global variables ========== */

extern reader_t mm_reader;
extern reader_t stdio_reader;
#ifdef HAVE_ZLIB
extern reader_t gz_reader;
#endif /* HAVE_ZLIB */
#ifdef HAVE_BZIP2
extern reader_t bz2_reader;
#endif /* HAVE_BZIP2 */

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

extern engine_t fixed_engine;
extern engine_t icure_engine;

engine_t *engines[] = {
    &fixed_engine,
    &icure_engine
};

int binbehave = BIN_FILE_SKIP;

// move to main()?
UBool nFlag = FALSE;
UBool vFlag = FALSE;
UBool wFlag = FALSE;

UBool print_file = TRUE; // -H/h
UBool colorize = TRUE;

/* ========== general helper functions ========== */

#ifdef DEBUG
const char *ubasename(const char *filename)
{
    const char *c;

    if (NULL == (c = strrchr(filename, '/'))) {
        return filename;
    } else {
        return c+1;
    }
}
#endif /* DEBUG */

static UBool stdout_is_tty(void)
{
    return (1 == isatty(STDOUT_FILENO));
}

static UBool stdin_is_tty(void) {
    return (1 == isatty(STDIN_FILENO));
}

static UBool is_binary_uchar(UChar32 c)
{
    return !u_isprint(c) && !u_isspace(c) && U_BS != c;
}

static UBool is_pattern(const UChar *pattern)
{
    // quotemeta : ".\+*?[^]($)"
    // PCRE : "\\+*?[^]$(){}=!<>|:-"
    const UChar meta[] = {
        0x005c, // esc
        0x002b, // +
        0x002a, // *
        0x003f, // ?
        0x005b, // [
        0x005d, // ]
        0x005e, // ^
        0x0024, // $
        0x0028, // (
        0x0029, // )
        0x007b, // {
        0x007d, // }
        0x003d, // =
        0x0021, // !
        0x003c, // <
        0x003e, // >
        0x007c, // |
        0x003a, // :
        0x002d, // -
        U_NUL
    };

    return (NULL != u_strpbrk(pattern, meta));
}

static UBool is_patternC(const char *pattern)
{
    return (NULL != strpbrk(pattern, "\\+*?[^]$(){}=!<>|:-"));
}

/* ========== fd helper functions ========== */

typedef struct {
    const char *filename;
    const char *encoding;
    reader_t *reader;
    void *reader_data;
    size_t filesize;
    size_t lineno;
    size_t matches;
    UBool binary;
} fd_t;

UBool fd_open(fd_t *fd, const char *filename)
{
    int fsfd;
    /*struct stat st;*/
    UErrorCode status;
    const char *encoding;
    int32_t signature_length;
    char buffer[MAX_ENC_REL_LEN + 1];
    size_t buffer_len;

    /*if (-1 == (stat(filename, &st))) {
        msg("can't stat %s: %s", filename, strerror(errno));
        return FALSE;
    }*/

    fd->reader_data = NULL;

    if (!strcmp("-", filename)) {
        if (!stdin_is_tty()) {
            msg("Sorry: cannot work with redirected and/or piped stdin (not seekable)");
            goto failed;
        }
        fd->filename = "(standard input)";
        fd->reader = &stdio_reader;
        fsfd = STDIN_FILENO;
    } else {
        fd->filename = filename;
        if (-1 == (fsfd = open(filename, O_RDONLY))) {
            msg("can't open %s: %s", filename, strerror(errno));
            goto failed;
        }
    }

    if (NULL == (fd->reader_data = fd->reader->open(filename, fsfd))) {
        goto failed;
    }

    encoding = NULL;
    signature_length = 0;
    status = U_ZERO_ERROR;
    fd->lineno = 0;
    /*fd->filesize = st.st_size;*/
    fd->matches = 0;
    fd->binary = FALSE;

    if (fd->reader->seekable(fd->reader_data)) {
        if (0 == (buffer_len = fd->reader->readbytes(fd->reader_data, buffer, MAX_ENC_REL_LEN))) {
            goto failed;
        }
        buffer[buffer_len] = '\0';
        encoding = ucnv_detectUnicodeSignature(buffer, buffer_len, &signature_length, &status);
        if (U_SUCCESS(status)) {
            if (NULL == encoding) {
                UCharsetDetector *csd;
                const UCharsetMatch *ucm;

                csd = ucsdet_open(&status);
                if (U_FAILURE(status)) {
                    icu(status, "ucsdet_open");
                    goto failed;
                }
                ucsdet_setText(csd, buffer, buffer_len, &status);
                if (U_FAILURE(status)) {
                    icu(status, "ucsdet_setText");
                    goto failed;
                }
                ucm = ucsdet_detect(csd, &status);
                if (U_FAILURE(status)) {
                    icu(status, "ucsdet_detect");
                    goto failed;
                }
                encoding = ucsdet_getName(ucm, &status);
                if (U_FAILURE(status)) {
                    icu(status, "ucsdet_getName");
                    goto failed;
                }
                ucsdet_close(csd);
            } else {
                fd->reader->set_signature_length(fd->reader_data, signature_length);
            }
            debug("%s, file encoding = %s", filename, encoding);
            fd->encoding = encoding;
            fd->reader->set_encoding(fd->reader_data, encoding); // a tester ?
            fd->reader->rewind(fd->reader_data);
        } else {
            icu(status, "ucnv_detectUnicodeSignature");
            goto failed;
        }
    }

    return TRUE;
failed:
    if (NULL != fd->reader_data) {
        fd->reader->close(fd->reader_data);
    }
    return FALSE;
}

int fd_is_binary(fd_t *fd)
{
    UChar32 *p;
    size_t buffer_len;
    UChar32 buffer[MAX_BIN_REL_LEN + 1];

    if (fd->reader->seekable(fd->reader_data)) {
        buffer_len = fd->reader->readuchars(fd->reader_data, buffer, MAX_BIN_REL_LEN);
        buffer[buffer_len] = U_NUL;
        // controler buffer_len
        for (p = buffer; U_NUL != *p; p++) {
            if (is_binary_uchar(*p)) {
                return TRUE;
            }
        }

        return (p - buffer) < buffer_len;
    } else {
        return FALSE; // In stdin we trust
    }
}

void fd_close(fd_t *fd)
{
    fd->reader->close(fd->reader_data);
    free(fd->reader_data);
}

void fd_rewind(fd_t *fd)
{
    if (fd->reader->seekable(fd->reader_data)) {
        fd->reader->rewind(fd->reader_data);
    }
}

int fd_readline(fd_t *fd, UString *ustr)
{
    ustring_truncate(ustr);
    return !fd->reader->eof(fd->reader_data) && fd->reader->readline(fd->reader_data, ustr);
}

/* ========== getopt stuff ========== */

enum {
    COLOR_OPT = CHAR_MAX + 1,
    BINARY_OPT,
    READER_OPT
};

static char optstr[] = "EFHVce:f:hinqsvwx";

static struct option long_options[] =
{
    {"color",           required_argument, NULL, COLOR_OPT},
    {"colour",          required_argument, NULL, COLOR_OPT},
    {"binary-files",    required_argument, NULL, BINARY_OPT},
    {"reader",          required_argument, NULL, READER_OPT},
    {"extended-regexp", no_argument,       NULL, 'E'}, // POSIX
    {"fixed-string",    no_argument,       NULL, 'F'}, // POSIX
    {"with-filename",   no_argument,       NULL, 'H'},
    {"version",         no_argument,       NULL, 'V'},
    {"count",           no_argument,       NULL, 'c'}, // POSIX
    {"regexp",          required_argument, NULL, 'e'}, // POSIX
    {"file",            required_argument, NULL, 'f'}, // POSIX
    {"no-filename",     no_argument,       NULL, 'h'},
    {"ignore-case",     no_argument,       NULL, 'i'}, // POSIX
    {"line-number",     no_argument,       NULL, 'n'}, // POSIX
    {"quiet",           no_argument,       NULL, 'q'}, // POSIX
    {"silent",          no_argument,       NULL, 'q'}, // POSIX
    {"no-messages",     no_argument,       NULL, 's'}, // POSIX
    {"revert-match",    no_argument,       NULL, 'v'}, // POSIX
    {"word-regexp",     no_argument,       NULL, 'w'},
    {"line-regexp",     no_argument,       NULL, 'x'}, // POSIX
    {NULL,              no_argument,       NULL, 0}
};

static void usage(void)
{
    extern char *__progname;

    fprintf(
        stderr,
        "usage: %s [-EFHVchinqsvwx]\n"
        "\t[-e pattern] [-f file] [--binary-files=value]\n"
        "\t[pattern] [file ...]\n",
        __progname
    );
    exit(EXIT_USAGE);
}

/* ========== adding patterns ========== */

void add_pattern(slist_t *l, const UChar *pattern, int32_t length, int pattern_type, UBool case_insensitive)
{
    void *data;
    pattern_data_t *pdata;

    pdata = mem_new(*pdata);
    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_pattern(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compile(pattern, length, case_insensitive))) {
        // TODO
        exit(EXIT_FAILURE);
    }
    pdata->pattern = data;
    pdata->engine = engines[!!pattern_type];

    slist_append(l, pdata);
}

void add_patternC(slist_t *l, const char *pattern, int pattern_type, UBool case_insensitive)
{
    void *data;
    pattern_data_t *pdata;

    pdata = mem_new(*pdata);
    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_patternC(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compileC(pattern, case_insensitive))) {
        // TODO
        exit(EXIT_FAILURE);
    }
    pdata->pattern = data;
    pdata->engine = engines[!!pattern_type];

    slist_append(l, pdata);
}

void source_patterns(const char *filename, slist_t *l, int pattern_type, UBool case_insensitive)
{
    fd_t fd;
    UString *ustr;

    fd.reader = &stdio_reader;

    ustr = ustring_new();
    if (!fd_open(&fd, filename)) {
        // TODO
        exit(EXIT_FAILURE);
    }
    while (fd_readline(&fd, ustr)) {
        ustring_chomp(ustr);
        add_pattern(l, ustr->ptr, ustr->len, pattern_type, case_insensitive);
    }
    fd_close(&fd);
    ustring_destroy(ustr);
}

#ifdef INTERVAL_TEST
typedef struct {
    int32_t lower_limit;
    int32_t upper_limit;
} interval_t;

interval_t *interval_new(int32_t lower_limit, int32_t upper_limit)
{
    interval_t *i;

    i = mem_new(*i);
    i->lower_limit = lower_limit;
    i->upper_limit = upper_limit;

    return i;
}

void interval_add_after(slist_t *UNUSED(intervals), slist_element_t *ref, int32_t lower_limit, int32_t upper_limit)
{
    slist_element_t *newel;

    newel = mem_new(*newel);
    newel->data = interval_new(lower_limit, upper_limit);
    newel->next = ref->next;
    ref->next = newel;
}

void interval_append(slist_t *intervals, int32_t lower_limit, int32_t upper_limit)
{
    slist_append(intervals, interval_new(lower_limit, upper_limit));
}

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif /* !MAX */

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* !MIN */

#define BETWEEN(value, lower, upper) \
    ((lower <= value) && (value <= upper))

#define IN(value, interval) \
    (((value) >= (interval)->lower_limit) && ((value) <= (interval)->upper_limit))

void interval_add(slist_t *intervals, int32_t lower_limit, int32_t upper_limit)
{
    slist_element_t *prev, *from, *to;

    printf("\nSEARCH [%d;%d]\n", lower_limit, upper_limit);
    if (slist_empty(intervals)) {
        interval_append(intervals, lower_limit, upper_limit);
    } else {
        for (from = intervals->head, prev = NULL; from; from = from->next) {
            FETCH_DATA(from->data, i, interval_t);

            printf("FOR1 [%d;%d]\n", i->lower_limit, i->upper_limit);
            if (IN(lower_limit, i) || IN(upper_limit, i)) {
                printf("FOR1, COND1\n");
                break;
            }
            if (lower_limit < i->lower_limit) {
                printf("FOR1, COND2\n");
                //from = prev;
                break;
            }
            prev = from;
        }
        if (!from) {
            printf("from = null\n");
            interval_append(intervals, lower_limit, upper_limit);
        } else {
            for (to = from->next, prev = NULL; to; to = to->next) {
                FETCH_DATA(to->data, i, interval_t);

                printf("FOR2 [%d;%d]\n", i->lower_limit, i->upper_limit);
                //if (!IN(lower_limit, i) && !IN(upper_limit, i)) {
                /*if (!BETWEEN(i->lower_limit, lower_limit, upper_limit) && !BETWEEN(i->upper_limit, lower_limit, upper_limit)) {
                    printf("FOR2, COND1\n");
                    break;
                }
                if (i->upper_limit > upper_limit) {*/
                if (i->upper_limit >= upper_limit && !BETWEEN(i->lower_limit, lower_limit, upper_limit) && !BETWEEN(i->upper_limit, lower_limit, upper_limit)) {
                    printf("FOR2, COND2\n");
                    to = prev;
                    break;
                }
                prev = to;
            }
            if (prev) {
                if (!to) {
                    to = prev;
                }
                {
                    slist_element_t *el;
                    FETCH_DATA(to->data, t, interval_t);
                    FETCH_DATA(from->data, f, interval_t);

                    printf("FROM [%d;%d]\n", f->lower_limit, f->upper_limit);
                    printf("TO [%d;%d]\n", t->lower_limit, t->upper_limit);
                    f->lower_limit = MIN(f->lower_limit, lower_limit);
                    f->upper_limit = MAX(t->upper_limit, upper_limit);
                    for (el = from->next, prev = from; el; ) {
                        slist_element_t *tmp = el;
                        el = el->next;
                        if (NULL != intervals->dtor_func) {
                            intervals->dtor_func(tmp->data);
                        }
                        prev->next = tmp->next;
                        if (tmp == to) {
                            free(tmp);
                            break;
                        }
                        free(tmp);
                    }
                }
            } else {
                FETCH_DATA(from->data, i, interval_t);
                printf("(prev = null) merge = 1\n");
                i->lower_limit = MIN(i->lower_limit, lower_limit);
                i->upper_limit = MAX(i->upper_limit, upper_limit);
            }
        }
    }
}
#endif /* INTERVAL_TEST */

/* ========== main ========== */

int main(int argc, char **argv)
{
    enum {
        COLOR_AUTO,
        COLOR_NEVER,
        COLOR_ALWAYS
    };

    int c;
    fd_t fd;
    int color;
    UString *ustr;
    UFILE *ustdout;
    UErrorCode status;
    slist_t *patterns;
    slist_element_t *p;
    reader_t *default_reader;

    UBool xFlag, iFlag;
    int pattern_type; // -E/F

    iFlag = xFlag = FALSE;
    color = COLOR_AUTO;
    status = U_ZERO_ERROR;
    patterns = slist_new(NULL);
    pattern_type = PATTERN_AUTO;
    default_reader = &mm_reader;
    ustdout = u_finit(stdout, NULL, NULL);

    debug("system locale = %s", u_fgetlocale(ustdout));
    debug("system codepage = %s", u_fgetcodepage(ustdout));

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'E':
                pattern_type = PATTERN_REGEXP;
                break;
            case 'F':
                pattern_type = PATTERN_LITERAL;
                break;
            case 'H':
                print_file = TRUE;
                break;
            case 'V':
                fprintf(stderr, "ugrep version %u.%u\n", UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
            case 'e':
                add_patternC(patterns, optarg, pattern_type, iFlag); // return value? (errors)
                break;
            case 'f':
                source_patterns(optarg, patterns, pattern_type, iFlag); // return value? (errors)
                break;
            case 'h':
                print_file = FALSE;
                break;
            case 'i':
                iFlag = TRUE;
                break;
            case 'n':
                nFlag = TRUE;
                break;
            case 'v':
                vFlag = TRUE;
                break;
            case 'w':
                // TODO: word-regexp
                break;
            case 'x':
                xFlag = TRUE;
                break;
            case COLOR_OPT:
                if (!strcmp("never", optarg)) {
                    color = COLOR_NEVER;
                } else if (!strcmp("auto", optarg)) {
                    color = COLOR_AUTO;
                } else if (!strcmp("always", optarg)) {
                    color = COLOR_ALWAYS;
                } else {
                    fprintf(stderr, "Unknown colo(u)r option\n");
                    return EXIT_USAGE;
                }
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
                    return EXIT_USAGE;
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
                        return EXIT_USAGE;
                    }
                }
                break;
            default:
                usage();
                break;
        }
        // XXX
    }
    argc -= optind;
    argv += optind;

    colorize = (COLOR_ALWAYS == color) || (COLOR_AUTO == color && stdout_is_tty());

    if (slist_empty(patterns)) {
        if (argc < 1) {
            usage();
        } else {
            add_patternC(patterns, *argv++, pattern_type, iFlag);
            argc--;
        }
    }

    ustr = ustring_new();
    for ( ; argc--; ++argv) {
        fd.reader = default_reader;
        if (fd_open(&fd, *argv)) {
            if (BIN_FILE_TEXT != binbehave) {
                fd.binary = fd_is_binary(&fd);
                debug("%s, binary file : %s", *argv, fd.binary ? RED("yes") : GREEN("no"));
                if (fd.binary) {
                    if (BIN_FILE_SKIP == binbehave) {
                        goto endloop;
                    }
                }
                fd_rewind(&fd);
            }
            while (fd_readline(&fd, ustr)) {
                fd.lineno++;
                ustring_chomp(ustr);
                for (p = patterns->head; NULL != p; p = p->next) {
                    FETCH_DATA(p->data, pdata, pattern_data_t);
                    // <very bad: drop this ASAP!>
/*
For fixed string, make a lowered copy of ustr which on working
*/
                    if (!xFlag) {
                        pdata->engine->pre_exec(pdata->pattern, ustr);
                    }
                    // </very bad: drop this ASAP!>
                    if (xFlag ? pdata->engine->whole_line_match(pdata->pattern, ustr) : pdata->engine->match(pdata->pattern, ustr)) {
                        fd.matches++; // increment just once per line
                        if (!vFlag) {
                            if (print_file) {
                                u_fprintf(ustdout, "\x1b[35m%s\x1b[0m:", fd.filename);
                            }
                            if (nFlag) {
                                u_fprintf(ustdout, "\x1b[32m%d\x1b[0m:", fd.lineno);
                            }
                            u_fputs(ustr->ptr, ustdout);
                        }
                    } else {
                        if (vFlag) {
                            if (print_file) {
                                u_fprintf(ustdout, "\x1b[35m%s\x1b[0m:", fd.filename);
                            }
                            if (nFlag) {
                                u_fprintf(ustdout, "\x1b[32m%d\x1b[0m:", fd.lineno);
                            }
                            u_fputs(ustr->ptr, ustdout);
                        }
                    }
                }
            }
endloop:
            fd_close(&fd);
        }
    }

    ustring_destroy(ustr);

    // put this in a destructor function
    for (p = patterns->head; NULL != p; p = p->next) {
        FETCH_DATA(p->data, pdata, pattern_data_t);
        pdata->engine->destroy(pdata->pattern);
    }

    slist_destroy(patterns);

#ifdef INTERVAL_TEST
    {
        slist_t *intervals;

        intervals = slist_new(NULL);

//         interval_add(intervals, 0, 4);
//         interval_add(intervals, 8, 12);
//         interval_add(intervals, 3, 9);

//         interval_add(intervals, 0, 1);
//         interval_add(intervals, 2, 3);
//         interval_add(intervals, 5, 6);
//         interval_add(intervals, 8, 9);
//         interval_add(intervals, 3, 1000);

        interval_add(intervals,  0,  2);
        interval_add(intervals,  4,  6);
        interval_add(intervals,  8, 10);
        interval_add(intervals, 12, 14);
        interval_add(intervals, 16, 18);
        interval_add(intervals, 20, 22);
        interval_add(intervals, 13, 1000);

        {
            slist_element_t *el;

            for (el = intervals->head; el; el = el->next) {
                FETCH_DATA(el->data, i, interval_t);
                printf("[%d;%d]\n", i->lower_limit, i->upper_limit);
            }
        }

        slist_destroy(intervals);
    }
#endif /* INTERVAL_TEST */

    return EXIT_SUCCESS;
}

/*
matching/printing process :
!colorize : stop and print at the first match
colorize : search all matches but matches should be treated by range before colorize them else result should be irrelevant
           (eg : echo eleve | grep -e el -e le)
           except for whole line matching: stop and print at the first match
*/
