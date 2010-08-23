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

#define RED(str) "\e[1;31m" str "\e[0m"
#define GREEN(str) "\e[1;32m" str "\e[0m"

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

UBool nFlag = FALSE; // move to main()?
UBool vFlag = FALSE; // move to main()?
UBool wFlag = FALSE; // move to main()?
UBool iFlag = FALSE; // move to main()?

UBool print_file = TRUE; // -H/h
UBool colorize = TRUE;

/* ========== general helper functions ========== */

static UBool stdout_is_tty()
{
    return (1 == isatty(STDOUT_FILENO));
}

UBool is_binary_uchar(UChar32 c)
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

    if (NULL == (fd->reader_data = fd->reader->open(filename))) {
        goto failed;
    }

    encoding = NULL;
    signature_length = 0;
    status = U_ZERO_ERROR;
    fd->filename = filename;
    fd->lineno = 0;
    /*fd->filesize = st.st_size;*/
    fd->matches = 0;
    fd->binary = FALSE;

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

    return TRUE;
failed:
    if (NULL != fd->reader_data) {
        fd->reader->close(fd->reader_data);
    }
    return FALSE;
}

#ifdef WITH_IS_BINARY
int fd_is_binary(fd_t *fd)
{
    return fd->reader->is_binary(fd->reader_data, MAX_BIN_REL_LEN);
}
#else
int fd_is_binary(fd_t *fd)
{
    UChar32 *p;
    size_t buffer_len;
    UChar32 buffer[MAX_BIN_REL_LEN + 1];

    buffer_len = fd->reader->readuchars(fd->reader_data, buffer, MAX_BIN_REL_LEN);
    buffer[buffer_len] = U_NUL;
    // controler buffer_len
    for (p = buffer; U_NUL != *p; p++) {
        if (is_binary_uchar(*p)) {
            return TRUE;
        }
    }

    return (p - buffer) < buffer_len;
}
#endif /* WITH_IS_BINARY */

void fd_close(fd_t *fd)
{
    fd->reader->close(fd->reader_data);
    free(fd->reader_data);
}

void fd_rewind(fd_t *fd)
{
    fd->reader->rewind(fd->reader_data);
}

int fd_readline(fd_t *fd, UString *ustr)
{
    ustring_truncate(ustr);
    return fd->reader->readline(fd->reader_data, ustr);
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

static void usage()
{
    fprintf(stderr, "usage: ME [pattern] [file]\n");
    exit(EXIT_USAGE);
}

/* ========== adding patterns ========== */

void add_pattern(slist_t *l, const UChar *pattern, int32_t length, int pattern_type)
{
    void *data;
    pattern_data_t *pdata;

    pdata = mem_new(*pdata);
    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_pattern(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compute(pattern, length))) {
        // TODO
        exit(EXIT_FAILURE);
    }
    pdata->pattern = data;
    pdata->engine = engines[!!pattern_type];

    slist_append(l, pdata);
}

void add_patternC(slist_t *l, const char *pattern, int pattern_type)
{
    void *data;
    pattern_data_t *pdata;

    pdata = mem_new(*pdata);
    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_pattern(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->computeC(pattern))) {
        // TODO
        exit(EXIT_FAILURE);
    }
    pdata->pattern = data;
    pdata->engine = engines[!!pattern_type];

    slist_append(l, pdata);
}

void source_patterns(const char *filename, slist_t *l, int pattern_type)
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
        add_pattern(l, ustr->ptr, ustr->len, pattern_type);
    }
    fd_close(&fd);
    ustring_destroy(ustr);
}

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

    UBool xFlag;
    int pattern_type; // -E/F

    fd.reader = &mm_reader;

    xFlag = FALSE;
    color = COLOR_AUTO;
    status = U_ZERO_ERROR;
    patterns = slist_new(NULL);
    pattern_type = PATTERN_AUTO;
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
                // TODO
                add_patternC(patterns, optarg, pattern_type);
                break;
            case 'f':
                source_patterns(optarg, patterns, pattern_type);
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

                    for (r = available_readers, fd.reader = NULL; *r; r++) {
                        if (!strcmp((*r)->name, optarg)) {
                            fd.reader = *r;
                            break;
                        }
                    }
                    if (NULL == fd.reader) {
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
        if (argc < 2) {
            usage();
        } else {
            add_patternC(patterns, *argv++, pattern_type);
            argc--;
        }
    }

    ustr = ustring_new();
    for ( ; argc--; ++argv) {
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
                    if (xFlag ? pdata->engine->whole_line_match(pdata->pattern, ustr) : pdata->engine->match(pdata->pattern, ustr)) {
                        fd.matches++; // increment just once per line
                        if (!vFlag) {
                            if (print_file) {
                                u_fprintf(ustdout, "\e[35m%s\e[0m:", fd.filename);
                            }
                            if (nFlag) {
                                u_fprintf(ustdout, "\e[32m%d\e[0m:", fd.lineno);
                            }
                            u_fputs(ustr->ptr, ustdout);
                        }
                    } else {
                        if (vFlag) {
                            if (print_file) {
                                u_fprintf(ustdout, "\e[35m%s\e[0m:", fd.filename);
                            }
                            if (nFlag) {
                                u_fprintf(ustdout, "\e[32m%d\e[0m:", fd.lineno);
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


#if 0
    if (argc != 2) {
        usage();
    } else {
        URegularExpression *uregex;

        {
            UParseError pe;

            uregex = uregex_openC(argv[0], reflags, &pe, &status);
            if (U_FAILURE(status)) {
                if (U_REGEX_RULE_SYNTAX == status) {
                    //u_fprintf(ustderr, "Error at offset %d %S %S\n", pe.offset, pe.preContext, pe.postContext);
                    fprintf(stderr, "Invalid pattern: error at offset %d\n", pe.offset);
                    fprintf(stderr, "\t%s\n", argv[1]);
                    fprintf(stderr, "\t%*c\n", pe.offset, '^');
                } else {
                    icu(status, "uregex_openC");
                }
                return EXIT_FAILURE;
            }
        }

        if (fd_open(&fd, argv[1])) {
            printf("Binaire : %s\n", fd_is_binary(&fd) ? "oui" : "non");
            {
                UChar replText[80], buf[80];
                UBool found;
                UString *ustr;

                u_uastrncpy(replText, "\e[1;31m$0\e[0m", sizeof(replText)/2);

                fd_rewind(&fd);

                ustr = ustring_new();
                while (fd_readline(&fd, ustr)) {
                    fd.lineno++;
                    ustring_chomp(ustr);
                    uregex_setText(uregex, ustr->ptr, ustr->len, &status);
                    if (U_FAILURE(status)) {
                        icu(status, "uregex_setText");
                        break; // TODO: gerer l'erreur
                    }
                    found = uregex_find(uregex, 0, &status);
                    if (U_FAILURE(status)) {
                        icu(status, "uregex_find");
                        break; // TODO: gerer l'erreur
                    }
                    if (found) {
                        fd.matches++;
                        if (!vflag) {
                            uregex_replaceAll(uregex, replText, -1, buf, sizeof(buf)/2, &status);
                            if (print_file) {
                                u_fprintf(ustdout, "\e[35m%s\e[0m:", fd.filename);
                            }
                            if (nflag) {
                                u_fprintf(ustdout, "\e[32m%d\e[0m:", fd.lineno);
                            }
                            u_fputs(buf, ustdout);
                        }
                    } else {
                        if (vflag) {
                            if (print_file) {
                                u_fprintf(ustdout, "\e[35m%s\e[0m:", fd.filename);
                            }
                            if (nflag) {
                                u_fprintf(ustdout, "\e[32m%d\e[0m:", fd.lineno);
                            }
                            u_fputs(ustr->ptr, ustdout);
                        }
                    }
                }
                ustring_destroy(ustr);
            }
            fd_close(&fd);
        }
        uregex_close(uregex);
    }
#endif

    return EXIT_SUCCESS;
}

/*
switch (binbehave) {
    case BIN_FILE_TEXT:
        // don't call fd_is_binary, treat it as text
        break;
    case BIN_FILE_BIN:
        // at first match: print match and continue
        break;
    case BIN_FILE_SKIP:
        // continue
        break;
    default:
        // bug !
}
*/

/*
matching/printing process :
!colorize : stop and print at the first match
colorize : search all match but matches should be treated by range before colorize them else result should be irrelevant
           (eg : echo eleve | grep -e el -e le)
*/
