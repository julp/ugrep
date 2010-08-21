#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include <unicode/ustdio.h>
#include <unicode/ucsdet.h>
#include <unicode/uregex.h>

#include "ugrep.h"

#define MAX_ENC_REL_LEN 4096 // Maximum relevant length for encoding analyse (in bytes)
#define MAX_BIN_REL_LEN 4096 // Maximum relevant length for binary analyse

#define EXIT_USAGE 2

#define RED(str) "\e[1;31m" str "\e[0m"
#define GREEN(str) "\e[1;32m" str "\e[0m"

/*
UChar* CharsToUChars(const char* str) {
    int32_t len = u_unescape(str, 0, 0);
    UChar *buf = (UChar*) malloc(sizeof(UChar) * (len + 1));
    u_unescape(str, buf, len + 1);
    return buf;
}
*/

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

UBool is_binary_uchar(UChar32 c)
{
    return !u_isprint(c) && !u_isspace(c) && U_BS != c;
}

extern reader_t mm_reader;
extern reader_t stdio_reader;
#ifdef HAVE_ZLIB
extern reader_t gz_reader;
#endif /* HAVE_ZLIB */
#ifdef HAVE_BZIP2
extern reader_t bz2_reader;
#endif /* HAVE_BZIP2 */

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
        debug("file encoding = %s", encoding);
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

enum {
    COLOR_OPT = CHAR_MAX + 1,
    BINARY_OPT,
    READER_OPT
};

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

enum {
    COLOR_AUTO,
    COLOR_NEVER,
    COLOR_ALWAYS
};

static char optstr[] = "EFHVce:f:hinqsvwx";

struct option long_options[] =
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

static UBool stdout_is_tty()
{
    return (1 == isatty(STDOUT_FILENO));
}

int binbehave = BIN_FILE_SKIP;

UBool nflag = FALSE;
UBool vflag = FALSE;
UBool wflag = FALSE;

UBool print_file = TRUE;
UBool colorize = TRUE;

typedef enum {
    E_FIXED_STR,
    E_ICU_REGEXP,
    _E_COUNT
} engine_type_t;

typedef struct {
    void *(*compute)(const UChar *, int32_t);
    void *(*computeC)(const char *);
    UBool (*match)(void *, const UString *);
    UBool (*whole_line_match)(void *, const UString *);
    //Range **(*get_match)(void *);
    void (*reset)(void *); // remove it?
    void (*destroy)(void *);
} engine_t;

typedef struct {
    void *pattern;
    //engine_type_t type;
    engine_t *engine;
} pattern_data_t;

void *engine_icure_compute(const UChar *upattern, int32_t length)
{
    UFILE *ustderr;
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    ustderr = u_finit(stdout, NULL, NULL);
    /* don't make a copy of upattern, ICU does this */
    uregex = uregex_open(upattern, length, /*TODO*/UREGEX_CASE_INSENSITIVE, &pe, &status);
    if (U_FAILURE(status)) {
        if (U_REGEX_RULE_SYNTAX == status) {
            //u_fprintf(ustderr, "Error at offset %d %S %S\n", pe.offset, pe.preContext, pe.postContext);
            u_fprintf(ustderr, "Invalid pattern: error at offset %d\n", pe.offset);
            u_fprintf(ustderr, "\t%S\n", upattern);
            u_fprintf(ustderr, "\t%*c\n", pe.offset, '^');
        } else {
            icu(status, "uregex_openC");
        }
        return NULL;
    }

    return uregex;
}

void *engine_icure_computeC(const char *pattern)
{
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    uregex = uregex_openC(pattern, /*TODO*/UREGEX_CASE_INSENSITIVE, &pe, &status);
    if (U_FAILURE(status)) {
        if (U_REGEX_RULE_SYNTAX == status) {
            //u_fprintf(ustderr, "Error at offset %d %S %S\n", pe.offset, pe.preContext, pe.postContext);
            fprintf(stderr, "Invalid pattern: error at offset %d\n", pe.offset);
            fprintf(stderr, "\t%s\n", pattern);
            fprintf(stderr, "\t%*c\n", pe.offset, '^');
        } else {
            icu(status, "uregex_openC");
        }
        return NULL;
    }

    return uregex;
}

UBool engine_icure_match(void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
    }
    ret = uregex_find(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_find");
    }

    return ret;
}

UBool engine_icure_whole_line_match(void *data, const UString *subject)
{
    FETCH_DATA(data, uregex, URegularExpression);
}

void engine_icure_reset(void *data)
{
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, 0, 0, &status);
}

void engine_icure_destroy(void *data)
{
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    uregex_close(uregex);
}

/* ========== */

UChar *ustrndup(const UChar *src, int32_t length)
{
    UChar *dst;

    dst = mem_new_n(*dst, length + 1);
    u_strncpy(dst, src, length);

    return dst;
}

void *engine_fixed_compute(const UChar *upattern, int32_t length)
{
    if (length < 0) {
        length = u_strlen(upattern);
    }
    return ustrndup(upattern, length);
}

void *engine_fixed_computeC(const char *pattern)
{
    UChar *upattern;
    UConverter *ucnv;
    UErrorCode status;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu(status, "ucnv_open");
        return NULL;
    }
    upattern = mem_new_n(*upattern, strlen(pattern) * ucnv_getMaxCharSize(ucnv) + 1);
    ucnv_close(ucnv);

    return upattern;
}

UBool engine_fixed_match(void *data, const UString *subject)
{
    FETCH_DATA(data, upattern, UChar);

    if (1/*iFlag*/) {
        // Case Insensitive version
    } else {
        return (NULL != u_strFindFirst(subject->ptr, subject->len, upattern, -1));
    }
}

UBool engine_fixed_whole_line_match(void *data, const UString *subject)
{
    FETCH_DATA(data, pattern, UChar);

    // u_str(case)cmp
}

void engine_fixed_reset(void *data)
{
    /* NOP */
    return;
}

void engine_fixed_destroy(void *data)
{
    FETCH_DATA(data, upattern, UChar);

    free(upattern);
}

engine_t engine_icure = {
    engine_icure_compute,
    engine_icure_computeC,
    engine_icure_match,
    engine_icure_whole_line_match,
    engine_icure_reset,
    engine_icure_destroy
};

engine_t engine_fixed = {
    engine_fixed_compute,
    engine_fixed_computeC,
    engine_fixed_match,
    engine_fixed_whole_line_match,
    engine_fixed_reset,
    engine_fixed_destroy
};

/* ========== */

typedef struct slist_t {
    struct slist_t *next;
    pattern_data_t *data;
} slist_t;

slist_t *patterns = NULL, *queue = NULL;

void add_pattern(const UChar *pattern, int32_t length/*, int type*/)
{
    void *data;
    slist_t *n;
    pattern_data_t *pdata;

    n = mem_new(*n);
    pdata = mem_new(*pdata);
    if (NULL == (data = engine_icure/*engine_fixed*/.compute(pattern, length))) {
        //
    }
    n->next = NULL;
    n->data = pdata;
    pdata->pattern = data;
    //pdata->type = E_ICU_REGEXP;
    pdata->engine = &engine_icure/*&engine_fixed*/;

    if (NULL != queue) {
        queue->next = n;

    } else {
        patterns = n;
    }
    queue = n;
}

void add_patternC(const char *pattern/*, int type*/)
{
    void *data;
    slist_t *n;
    pattern_data_t *pdata;

    n = mem_new(*n);
    pdata = mem_new(*pdata);
    if (NULL == (data = engine_icure.computeC(pattern))) {
        //
    }
    n->next = NULL;
    n->data = pdata;
    pdata->pattern = data;
    //pdata->type = E_ICU_REGEXP;
    pdata->engine = &engine_icure;

    if (NULL != queue) {
        queue->next = n;

    } else {
        patterns = n;
    }
    queue = n;
}

void source_patterns(const char *filename)
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
        add_pattern(ustr->ptr, ustr->len);
    }
    fd_close(&fd);
    ustring_destroy(ustr);
}

int main(int argc, char **argv)
{
    int c;
    fd_t fd;
    int color;
    slist_t *p;
    UString *ustr;
    //uint32_t reflags;
    UFILE *ustdout;
    UErrorCode status;

    fd.reader = &mm_reader;

    //reflags = 0;
    color = COLOR_AUTO;
    status = U_ZERO_ERROR;
    ustdout = u_finit(stdout, NULL, NULL);

    debug("default locale = %s", u_fgetlocale(ustdout));
    debug("stdout encoding = %s", u_fgetcodepage(ustdout));

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'E':
                // TODO
                break;
            case 'F':
                //reflags |= UREGEX_LITERAL; // Not implemented by ICU
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
                add_patternC(optarg/*, type*/);
                break;
            case 'f':
                source_patterns(optarg);
                break;
            case 'h':
                print_file = FALSE;
                break;
            case 'i':
                //reflags |= UREGEX_CASE_INSENSITIVE;
                break;
            case 'l':
                // TODO: line-regexp
                break;
            case 'n':
                nflag = TRUE;
                break;
            case 'v':
                vflag = TRUE;
                break;
            case 'w':
                // TODO: word-regexp
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
#if 0
                    struct readers {
                        const char *name;
                        reader_t *reader;
                    } available_readers[] = {
                        {"mmap",  &mm_reader},
                        {"stdio", &stdio_reader},
#ifdef HAVE_ZLIB
                        {"gzip", &compressedgz_reader},
#endif /* HAVE_ZLIB */
                        {NULL,    NULL}
                    }, *r;

                    for (r = available_readers, fd.reader = NULL; r->name && r->reader; r++) {
                        if (!strcmp(r->name, optarg)) {
                            fd.reader = r->reader;
                            break;
                        }
                    }
#endif
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
                    }, **r;

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

    if (NULL == queue) {
        if (argc < 2) {
            usage();
        } else {
            add_patternC(*argv++);
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
                for (p = patterns; NULL != p; p = p->next) {
                    if (p->data->engine->match(p->data->pattern, ustr)) {
                        fd.matches++; // increment just once per line ?
                        if (!vflag) {
                            if (print_file) {
                                u_fprintf(ustdout, "\e[35m%s\e[0m:", fd.filename);
                            }
                            if (nflag) {
                                u_fprintf(ustdout, "\e[32m%d\e[0m:", fd.lineno);
                            }
                            u_fputs(ustr->ptr, ustdout);
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
            }
endloop:
            fd_close(&fd);
        }
    }

    ustring_destroy(ustr);

    for (p = patterns; NULL != p; p = p->next) {
        p->data->engine->destroy(p->data->pattern);
    }


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
