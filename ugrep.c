#include <limits.h>
#ifdef _MSC_VER
# define STRICT
# include <windows.h>
# include <io.h>
# include <direct.h>
# include <shlobj.h>
#else
# include <sys/param.h>
# include <unistd.h>
# include <pwd.h>
# include <fts.h>
#endif /* _MSC_VER */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#include "ugrep.h"

#define MIN_CONFIDENCE  39   // Minimum confidence for a match (in percents)
#define MAX_ENC_REL_LEN 4096 // Maximum relevant length for encoding analyse (in bytes)
#define MAX_BIN_REL_LEN 4096 // Maximum relevant length for binary analyse

#define SEP_MATCH_UCHAR    0x003a
#define SEP_NO_MATCH_UCHAR 0x002d

#ifdef DEBUG
# define RED(str)    "\33[1;31m" str "\33[0m"
# define GREEN(str)  "\33[1;32m" str "\33[0m"
# define YELLOW(str) "\33[1;33m" str "\33[0m"
#else
# define RED(str)    str
# define GREEN(str)  str
# define YELLOW(str) str
#endif /* DEBUG */

enum {
    UGREP_EXIT_MATCH = 0,
    UGREP_EXIT_NO_MATCH = 1,
    UGREP_EXIT_FAILURE = 2,
    UGREP_EXIT_USAGE
};

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

typedef struct {
    UString *ustr;
    UBool no_match;
} line_t;

void *line_ctor(void) {
    line_t *l;

    l = mem_new(*l);
    l->ustr = ustring_new();

    return l;
}

void line_dtor(void *data) {
    FETCH_DATA(data, l, line_t);

    ustring_destroy(l->ustr);
    free(l);
}

/* ========== global variables ========== */

extern char *__progname;

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
extern engine_t re_engine;

engine_t *engines[] = {
    &fixed_engine,
    &re_engine
};

int binbehave = BIN_FILE_SKIP;

UFILE *ustdout = NULL, *ustderr = NULL;
//UString *ustr = NULL;
static fixed_circular_list_t *lines = NULL;
static slist_t *patterns = NULL;
static reader_t *default_reader = NULL;
#ifdef OLD_INTERVAL
static slist_t *intervals = NULL;
#else
static slist_pool_t *intervals = NULL;
#endif /* OLD_INTERVAL */

UBool rFlag = FALSE;
UBool xFlag = FALSE;
UBool nFlag = FALSE;
UBool vFlag = FALSE;
UBool cFlag = FALSE;
UBool lFlag = FALSE;
UBool LFlag = FALSE;

size_t after_context = 0;
size_t before_context = 0;

UBool file_print = FALSE; // -H/h
#ifndef NO_COLOR
UBool colorize = TRUE;
#endif /* !NO_COLOR */
UBool line_print = TRUE;

/* ========== general helper functions ========== */

#ifdef DEBUG
const char *ubasename(const char *filename)
{
    const char *c;

    if (NULL == (c = strrchr(filename, '/'))) {
        return filename;
    } else {
        return c + 1;
    }
}
#endif /* DEBUG */

#ifdef DEBUG
int verbosity = INFO;
#else
int verbosity = WARN;
#endif /* DEBUG */

void print_error(error_t *error)
{
    if (NULL != error && error->type >= verbosity) {
        int type;

        type = error->type;
        switch (type) {
            case WARN:
                u_fprintf(ustderr, "[ " YELLOW("WARN") " ] ");
                break;
            case FATAL:
                u_fprintf(ustderr, "[ " RED("ERR ") " ] ");
                break;
            default:
                type = FATAL;
                u_fprintf(ustderr, "[ " RED("BUG ") " ] Unknown error type for:\n");
                break;
        }
        u_fputs(error->message, ustderr);
        error_destroy(error);
        if (type == FATAL) {
            exit(UGREP_EXIT_FAILURE);
        }
    }
}

void report(int type, const char *format, ...)
{
    if (type >= verbosity) {
        va_list args;

        switch (type) {
            case INFO:
                fprintf(stderr, "[ " GREEN("INFO") " ] ");
                break;
            case WARN:
                fprintf(stderr, "[ " YELLOW("WARN") " ] ");
                break;
            case FATAL:
                fprintf(stderr, "[ " RED("ERR ") " ] ");
                break;
        }
        va_start(args, format);
        u_vfprintf(ustderr, format, args);
        va_end(args);
        if (type == FATAL) {
            exit(UGREP_EXIT_FAILURE);
        }
    }
}

#ifndef NO_COLOR
static UBool stdout_is_tty(void)
{
# ifdef _MSC_VER
    return _isatty(_fileno(stdout));
# else
    return (1 == isatty(STDOUT_FILENO));
# endif /* _MSC_VER */
}
#endif /* !NO_COLOR */

static UBool stdin_is_tty(void) {
#ifdef _MSC_VER
    return _isatty(_fileno(stderr));
#else
    return (1 == isatty(STDIN_FILENO));
#endif /* _MSC_VER */
}

static UBool is_binary_uchar(UChar32 c)
{
    return !u_isprint(c) && !u_isspace(c) && U_BS != c;
}

static UBool is_binary(UChar32 *buffer, size_t buffer_len)
{
    UChar32 *p;

    for (p = buffer; U_NUL != *p; p++) {
        if (is_binary_uchar(*p)) {
            return TRUE;
        }
    }

    return (p - buffer) < buffer_len;
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

void fd_close(fd_t *fd)
{
    fd->reader->close(fd->reader_data);
    free(fd->reader_data);
}

UBool fd_open(error_t **error, fd_t *fd, const char *filename)
{
    int fsfd;
    /*struct stat st;*/
    UErrorCode status;
    size_t buffer_len;
    const char *encoding;
    int32_t signature_length;
    char buffer[MAX_ENC_REL_LEN + 1];

    /*if (-1 == (stat(filename, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        return FALSE;
    }*/

    fd->reader_data = NULL;

    if (!strcmp("-", filename)) {
        if (!stdin_is_tty()) {
            error_set(error, WARN, "Sorry, can't work with redirected or piped stdin (not seekable, sources can use many codepage). Skip stdin.");
            goto failed;
        }
        fd->filename = "(standard input)";
        fd->reader = &stdio_reader;
#ifdef _MSC_VER
        fsfd = _fileno(stdout);
#else
        fsfd = STDIN_FILENO;
#endif /* _MSC_VER */
    } else {
        fd->filename = filename;
        if (-1 == (fsfd = open(filename, O_RDONLY))) {
            error_set(error, WARN, "can't open %s: %s", filename, strerror(errno));
            goto failed;
        }
    }

    if (NULL == (fd->reader_data = fd->reader->open(error, filename, fsfd))) {
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
        if (0 != (buffer_len = fd->reader->readbytes(fd->reader_data, buffer, MAX_ENC_REL_LEN))) {
            buffer[buffer_len] = '\0';
            encoding = ucnv_detectUnicodeSignature(buffer, buffer_len, &signature_length, &status);
            if (U_SUCCESS(status)) {
                if (NULL == encoding) {
                    int32_t confidence;
                    UCharsetDetector *csd;
                    const char *tmpencoding;
                    const UCharsetMatch *ucm;

                    csd = ucsdet_open(&status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_open");
                        goto failed;
                    }
                    ucsdet_setText(csd, buffer, buffer_len, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_setText");
                        goto failed;
                    }
                    ucm = ucsdet_detect(csd, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_detect");
                        goto failed;
                    }
                    confidence = ucsdet_getConfidence(ucm, &status);
                    tmpencoding = ucsdet_getName(ucm, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_getName");
                        ucsdet_close(csd);
                        goto failed;
                    }
                    if (confidence > MIN_CONFIDENCE) {
                        encoding = tmpencoding;
                        debug("%s, confidence of " GREEN("%d%%") " for " YELLOW("%s"), filename, confidence, tmpencoding);
                    } else {
                        debug("%s, confidence of " RED("%d%%") " for " YELLOW("%s"), filename, confidence, tmpencoding);
                        //encoding = "US-ASCII";
                    }
                    ucsdet_close(csd);
                } else {
                    fd->reader->set_signature_length(fd->reader_data, signature_length);
                }
                debug("%s, file encoding = %s", filename, encoding);
                fd->encoding = encoding;
                if (!fd->reader->set_encoding(error, fd->reader_data, encoding)) {
                    goto failed;
                }
                fd->reader->rewind(fd->reader_data);
                if (BIN_FILE_TEXT != binbehave) {
                    int32_t ubuffer_len;
                    UChar32 ubuffer[MAX_BIN_REL_LEN + 1];

                    if (-1 == (ubuffer_len = fd->reader->readuchars(error, fd->reader_data, ubuffer, MAX_BIN_REL_LEN))) {
                        goto failed;
                    }
                    ubuffer[ubuffer_len] = U_NUL;
                    fd->binary = is_binary(ubuffer, ubuffer_len);
                    debug("%s, binary file : %s", filename, fd->binary ? RED("yes") : GREEN("no"));
                    if (fd->binary) {
                        if (BIN_FILE_SKIP == binbehave) {
                            goto failed;
                        }
                    }
                    fd->reader->rewind(fd->reader_data);
                }
            } else {
                icu_error_set(error, WARN, status, "ucnv_detectUnicodeSignature");
                goto failed;
            }
        }
    }

    return TRUE;
failed:
    if (NULL != fd->reader_data) {
        //fd->reader->close(fd->reader_data);
        fd_close(fd);
    }
    return FALSE;
}

UBool fd_eof(fd_t *fd) {
    return fd->reader->eof(fd->reader_data);
}

UBool fd_readline(error_t **error, fd_t *fd, UString *ustr)
{
    ustring_truncate(ustr);
    return fd->reader->readline(error, fd->reader_data, ustr);
}

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = CHAR_MAX + 1,
#ifndef NO_COLOR
    COLOR_OPT,
#endif /* !NO_COLOR */
    READER_OPT
};

static char optstr[] = "A:B:C:EFHLRVce:f:hilnqrsvwx";

static struct option long_options[] =
{
#ifndef NO_COLOR
    {"color",               required_argument, NULL, COLOR_OPT},
    {"colour",              required_argument, NULL, COLOR_OPT},
#endif /* !NO_COLOR */
    {"binary-files",        required_argument, NULL, BINARY_OPT},
    {"reader",              required_argument, NULL, READER_OPT},
    {"after-context",       required_argument, NULL, 'A'},
    {"before-context",      required_argument, NULL, 'B'},
    {"context",             required_argument, NULL, 'C'},
    {"extended-regexp",     no_argument,       NULL, 'E'}, // POSIX
    {"fixed-string",        no_argument,       NULL, 'F'}, // POSIX
    {"with-filename",       no_argument,       NULL, 'H'},
    {"files-without-match", no_argument,       NULL, 'L'},
    {"recursive",           no_argument,       NULL, 'R'},
    {"version",             no_argument,       NULL, 'V'},
    {"count",               no_argument,       NULL, 'c'}, // POSIX
    {"regexp",              required_argument, NULL, 'e'}, // POSIX
    {"file",                required_argument, NULL, 'f'}, // POSIX
    {"no-filename",         no_argument,       NULL, 'h'},
    {"ignore-case",         no_argument,       NULL, 'i'}, // POSIX
    {"files-with-matches",  no_argument,       NULL, 'l'},
    {"line-number",         no_argument,       NULL, 'n'}, // POSIX
    {"quiet",               no_argument,       NULL, 'q'}, // POSIX
    {"silent",              no_argument,       NULL, 'q'}, // POSIX
    {"recursive",           no_argument,       NULL, 'r'},
    {"no-messages",         no_argument,       NULL, 's'}, // POSIX
    {"revert-match",        no_argument,       NULL, 'v'}, // POSIX
    {"word-regexp",         no_argument,       NULL, 'w'},
    {"line-regexp",         no_argument,       NULL, 'x'}, // POSIX
    {NULL,                  no_argument,       NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [-EFHLRVchilnqrsvwx] [-A num] [-B num]\n"
        "\t[-e pattern] [-f file] [--binary-files=value]\n"
        "\t[pattern] [file ...]\n",
        __progname
    );
    exit(UGREP_EXIT_USAGE);
}

/* ========== adding patterns ========== */

# define OPTIONS_TO_ENGINE_FLAGS(flags, iFlag, wFlag, xFlag) \
    flags = ((iFlag ? OPT_CASE_INSENSITIVE : 0) | (xFlag ? OPT_WHOLE_LINE_MATCH : 0) | (wFlag ? OPT_WORD_BOUND : 0)); \
    if ((flags & (OPT_WHOLE_LINE_MATCH | OPT_WORD_BOUND)) == (OPT_WHOLE_LINE_MATCH | OPT_WORD_BOUND)) { \
        flags &= ~OPT_WORD_BOUND; \
    }

UBool add_pattern(error_t **error, slist_t *l, const UChar *pattern, int32_t length, int pattern_type, uint32_t flags)
{
    void *data;
    pattern_data_t *pdata;

    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_pattern(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compile(error, pattern, length, flags))) {
        return FALSE;
    }
    pdata = mem_new(*pdata);
    pdata->pattern = data;
    pdata->engine = engines[!!pattern_type];

    slist_append(l, pdata);

    return TRUE;
}

UBool add_patternC(error_t **error, slist_t *l, const char *pattern, int pattern_type, uint32_t flags)
{
    void *data;
    pattern_data_t *pdata;

    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_patternC(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compileC(error, pattern, flags))) {
        return FALSE;
    }
    pdata = mem_new(*pdata);
    pdata->pattern = data;
    pdata->engine = engines[!!pattern_type];

    slist_append(l, pdata);

    return TRUE;
}

UBool source_patterns(error_t **error, const char *filename, slist_t *l, int pattern_type, uint32_t flags)
{
    fd_t fd;
    UBool retval;
    UString *ustr;

    retval = TRUE;
    fd.reader = &stdio_reader;
    if (!fd_open(error, &fd, filename)) {
        return FALSE;
    }
    ustr = ustring_new();
    while (retval && !fd_eof(&fd)) {
        if(!fd_readline(error, &fd, ustr)) {
            retval = FALSE;
        } else {
            ustring_chomp(ustr);
            if (!add_pattern(error, l, ustr->ptr, ustr->len, pattern_type, flags)) {
                retval = FALSE;
            }
        }
    }
    fd_close(&fd);
    ustring_destroy(ustr);

    return retval;
}

static void pattern_destroy(void *data)
{
    FETCH_DATA(data, p, pattern_data_t);
    p->engine->destroy(p->pattern);
    free(data);
}

/* ========== highlighting ========== */

#ifndef NO_COLOR
typedef struct {
    const char *name;
    int fg;
    int bg;
} attr_t;

static const UChar reset[] = {0x001b, 0x005b, 0x0030, 0x006d, U_NUL};
static int32_t reset_len = ARRAY_SIZE(reset) - 1;

attr_t attrs[] = {
    {"bright",     1,  1},
    {"bold",       1,  1},
    {"faint",      2,  2},
    {"italic",     3,  3},
    {"underline",  4,  4},
    {"blink",      5,  5},
    {"overline",   6,  6},
    {"reverse",    7,  7},
    {"invisible",  8,  8},
    {"black",     30, 40},
    {"red",       31, 41},
    {"green",     32, 42},
    {"yellow",    33, 43},
    {"blue",      34, 44},
    {"magenta",   35, 45},
    {"cyan",      36, 46},
    {"white",     37, 47},
    {"default",   39, 49},
    {NULL,        -1, -1}
};

typedef enum {
    SINGLE_MATCH,
    LINE_MATCH,
    FILE_MATCH,
    FILE_NO_MATCH,
    LINE_NUMBER,
    SEP_MATCH,
    SEP_NO_MATCH,
    CONTEXT_SEP
} color_type_t;

# define MAX_ATTRS   8
# define MAX_SEQ_LEN 64

typedef struct {
    const char *name;
    UChar value[MAX_SEQ_LEN];
} color_t;

color_t colors[] = {
    {"single-match",  {0x001b, 0x005b, 0x0031, 0x003b, 0x0033, 0x0034, 0x006d, U_NUL}},
    {"line-match",    {0x001b, 0x005b, 0x0031, 0x003b, 0x0033, 0x0036, 0x006d, U_NUL}},
    {"file-match",    {0x001b, 0x005b, 0x0033, 0x0032, 0x006d, U_NUL}},
    {"file-no-match", {0x001b, 0x005b, 0x0033, 0x0031, 0x006d, U_NUL}},
    {"line-number",   {0x001b, 0x005b, 0x0033, 0x0035, 0x006d, U_NUL}},
    {"sep-match",     {0x001b, 0x005b, 0x0033, 0x0033, 0x006d, U_NUL}},
    {"sep-no-match",  {0x001b, 0x005b, 0x0033, 0x0033, 0x006d, U_NUL}},
    {"context-sep",   {0x001b, 0x005b, 0x0033, 0x0033, 0x006d, U_NUL}},
    {NULL,            {U_NUL}}
};

static UChar *u_stpncpy(UChar *dest, const UChar *src, int32_t length)
{
    int32_t n = u_strlen(src);

    if (n > length) {
        return NULL;
    }

    return u_strncpy(dest, src, length) + n;
}

static void parse_userpref(void)
{
    char *home;

    if (NULL == (home = getenv("HOME"))) {
# ifdef _MSC_VER
#  ifndef CSIDL_PROFILE
#   define CSIDL_PROFILE 40
#  endif /* CSIDL_PROFILE */
        if (NULL == (home = getenv("USERPROFILE"))) {
            HRESULT hr;
            LPITEMIDLIST pidl = NULL;

            hr = SHGetSpecialFolderLocation (NULL, CSIDL_PROFILE, &pidl);
            if (hr == S_OK) {
                SHGetPathFromIDList(pidl, home);
                CoTaskMemFree (pidl);
            }
        }
# else
        struct passwd *pwd;

        if (NULL != (pwd = getpwuid(getuid()))) {
            home = pwd->pw_dir;
        }
# endif /* _MSC_VER */
    }

    if (NULL != home) {
        char preffile[MAXPATHLEN];

        if (snprintf(preffile, sizeof(preffile), "%s%c%s", home, '/', ".ugrep") < sizeof(preffile)) {
            struct stat st;

            if ((-1 != (stat(preffile, &st))) && S_ISREG(st.st_mode)) {
                FILE *fp;

                if (NULL != (fp = fopen(preffile, "r"))) {
                    char line[4096];

                    while (NULL != fgets(line, sizeof(line), fp)) {
                        color_t *c;

                        if ('\n' == *line || '#' == *line) {
                            continue;
                        }
                        for (c = colors; c->name; c++) {
                            if (!strncmp(c->name, line, strlen(c->name))) {
                                char *s, *t;
                                int attrs_count, user_attrs[MAX_ATTRS], colors_count;

                                s = line + strlen(c->name);
                                attrs_count = colors_count = 0;
                                do {
                                    while (isblank(*s) || ispunct(*s)) {
                                        s++;
                                    }
                                    t = s;
                                    if (isdigit(*s)) {
                                        while (isdigit(*++s))
                                            ;
                                        if (s > t) {
                                            long val;
                                            char *endptr;

                                            *s++ = 0; // TODO: is it safe?
                                            errno = 0;
                                            val = strtol(t, &endptr, 10);
                                            if (0 != errno || endptr == t || *endptr != '\0') {
                                                goto nextline;
                                            } else {
                                                attr_t *a;

                                                for (a = attrs; a->name; a++) {
                                                    if (val == a->bg || val == a->fg) {
                                                        if (a->bg == a->fg || (a->bg != a->fg && colors_count++ < 2)) {
                                                            user_attrs[attrs_count++] = val;
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    } else if (islower(*s)) {
                                        while (islower(*++s))
                                            ;
                                        if (s > t) {
                                            attr_t *a;

                                            *s++ = 0; // TODO: is it safe?
                                            if (!strcmp("none", t)) {
                                                *c->value = U_NUL;
                                                break;
                                            } else {
                                                for (a = attrs; a->name; a++) {
                                                    if (!strcmp(a->name, t)) {
                                                        if (a->bg == a->fg || (a->bg != a->fg && colors_count < 2)) {
                                                            user_attrs[attrs_count++] = colors_count++ ? a->bg : a->fg;
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (attrs_count >= MAX_ATTRS) {
                                        break;
                                    }
                                } while (*s && '\n' != *s);
                                if (attrs_count > 0) {
                                    UChar prefix[] = {0x001b, 0x005b, U_NUL}, suffix[] = {0x006d, U_NUL}, sep[] = {0x003b, U_NUL};
                                    UChar *ptr, buf[MAX_SEQ_LEN], defval[MAX_SEQ_LEN];
                                    UFILE *ufp;
                                    int i, len;

                                    ptr = c->value;
                                    u_strcpy(defval, c->value);
                                    if (NULL != (ufp = u_fstropen(buf, sizeof(buf), NULL))) {
                                        if (NULL == (ptr = u_stpncpy(c->value, prefix, MAX_SEQ_LEN - (ptr - c->value)))) {
                                            u_strcpy(c->value, defval); // restore default color
                                            u_fclose(ufp);
                                            goto nextline;
                                        }
                                        for (i = 0; i < attrs_count; i++) {
                                            len = u_fprintf(ufp, "%d", user_attrs[i]);
                                            if (NULL == (ptr = u_stpncpy(ptr, sep, MAX_SEQ_LEN - (ptr - c->value)))) {
                                                u_strcpy(c->value, defval); // restore default color
                                                u_fclose(ufp);
                                                goto nextline;
                                            }
                                            if (NULL == (ptr = u_stpncpy(ptr, buf, MAX_SEQ_LEN - (ptr - c->value)))) {
                                                u_strcpy(c->value, defval); // restore default color
                                                u_fclose(ufp);
                                                goto nextline;
                                            }
                                            u_frewind(ufp);
                                        }
                                        u_fclose(ufp);
                                        if (NULL == (ptr = u_stpncpy(ptr, suffix, MAX_SEQ_LEN - (ptr - c->value)))) {
                                            u_strcpy(c->value, defval); // restore default color
                                            goto nextline;
                                        }
                                    }
                                }
                            }
                        }
nextline:
                        ;
                    }
                }
            }
        }
    }
}
#endif /* !NO_COLOR */

/* ========== process on file and/or directory helper ========== */

static void print_file(const char *filename, UBool no_file_match, UBool no_line_match, UBool print_sep, UBool eol)
{
#ifndef NO_COLOR
    if (colorize && *colors[no_file_match ? FILE_NO_MATCH : FILE_MATCH].value) {
        u_file_write(colors[no_file_match ? FILE_NO_MATCH : FILE_MATCH].value, -1, ustdout);
    }
#endif /* !NO_COLOR */
    u_fprintf(ustdout, "%s", filename);
#ifndef NO_COLOR
    if (colorize && *colors[no_file_match ? FILE_NO_MATCH : FILE_MATCH].value) {
        u_file_write(reset, reset_len, ustdout);
    }
#endif /* !NO_COLOR */
    if (print_sep) {
#ifndef NO_COLOR
        if (colorize && *colors[no_line_match ? SEP_NO_MATCH : SEP_MATCH].value) {
            u_file_write(colors[no_line_match ? SEP_NO_MATCH : SEP_MATCH].value, -1, ustdout);
        }
#endif /* !NO_COLOR */
        u_fputc(no_line_match ? SEP_NO_MATCH_UCHAR : SEP_MATCH_UCHAR, ustdout);
#ifndef NO_COLOR
        if (colorize && *colors[no_line_match ? SEP_NO_MATCH : SEP_MATCH].value) {
            u_file_write(reset, reset_len, ustdout);
        }
#endif /* !NO_COLOR */
    }
    if (eol) {
        u_fputc(U_LF, ustdout);
    }
}

static void print_line(int lineno, UBool no_line_match, UBool print_sep, UBool eol)
{
#ifndef NO_COLOR
    if (colorize && *colors[LINE_NUMBER].value) {
        u_file_write(colors[LINE_NUMBER].value, -1, ustdout);
    }
#endif /* !NO_COLOR */
    u_fprintf(ustdout, "%d", lineno);
#ifndef NO_COLOR
    if (colorize && *colors[LINE_NUMBER].value) {
        u_file_write(reset, reset_len, ustdout);
    }
#endif /* !NO_COLOR */
    if (print_sep) {
#ifndef NO_COLOR
        if (colorize && *colors[no_line_match ? SEP_NO_MATCH : SEP_MATCH].value) {
            u_file_write(colors[no_line_match ? SEP_NO_MATCH : SEP_MATCH].value, -1, ustdout);
        }
#endif /* !NO_COLOR */
        u_fputc(no_line_match ? SEP_NO_MATCH_UCHAR : SEP_MATCH_UCHAR, ustdout);
#ifndef NO_COLOR
        if (colorize && *colors[no_line_match ? SEP_NO_MATCH : SEP_MATCH].value) {
            u_file_write(reset, reset_len, ustdout);
        }
#endif /* !NO_COLOR */
    }
    if (eol) {
        u_fputc(U_LF, ustdout);
    }
}

static int procfile(fd_t *fd, const char *filename)
{
    UString *ustr;
    error_t *error;
#ifndef NO_COLOR
    UBool _colorize;
#endif /* !NO_COLOR */
    size_t last_line_print = 0;
    int _after_context;

    error = NULL;
    _after_context = 0;
    fd->reader = default_reader; // Restore default (stdin requires a switch on stdio)
#ifndef NO_COLOR
    _colorize = colorize && (before_context || after_context || !vFlag);
#endif /* !NO_COLOR */

    fixed_circular_list_clean(lines);
    if (fd_open(&error, fd, filename)) {
        UBool _line_print; /* line_print local override */

        _line_print = line_print && (!fd->binary || (fd->binary && BIN_FILE_BIN != binbehave));
        while (!fd_eof(fd)) {
            int matches;
            slist_element_t *p;
            engine_return_t ret;
            FETCH_DATA(fixed_circular_list_fetch(lines), line, line_t);

            ustr = line->ustr;
            if (!fd_readline(&error, fd, ustr)) {
                print_error(error);
            }
            matches = 0;
            fd->lineno++;
            ustring_chomp(ustr);
            if (BIN_FILE_TEXT == binbehave) {
                ustring_dump(ustr);
            }
#ifdef OLD_INTERVAL
            slist_clean(intervals);
#else
            slist_pool_clean(intervals);
#endif /* OLD_INTERVAL */
            for (p = patterns->head; NULL != p; p = p->next) {
                FETCH_DATA(p->data, pdata, pattern_data_t);

                if (xFlag) {
                    ret = pdata->engine->whole_line_match(&error, pdata->pattern, ustr);
                } else {
#ifndef NO_COLOR
                    if (_colorize && _line_print) {
                        ret = pdata->engine->match_all(&error, pdata->pattern, ustr, intervals);
                    } else {
#endif /* !NO_COLOR */
                        ret = pdata->engine->match(&error, pdata->pattern, ustr);
#ifndef NO_COLOR
                    }
#endif /* !NO_COLOR */
                }
                if (ENGINE_FAILURE == ret) {
                    print_error(error);
                } else if (ENGINE_WHOLE_LINE_MATCH == ret) {
                    matches++;
                    break; // no need to continue (line level)
                } else {
                    //matches += ret;
                    //matches += (ret && !vFlag) || (!ret && vFlag);
                    if (!vFlag) {
                        matches += ret;
                    } else {
                        matches += !ret;
                    }
                }
                //if (matches > 0 && (lFlag || (!vFlag && fd->binary && BIN_FILE_BIN == binbehave))) {
                if (matches && (lFlag || (fd->binary && BIN_FILE_BIN == binbehave))) {
                    debug("file skipping");
                    fd->matches = 1;
                    goto endfile; // no need to continue (file level)
                }
            }
            fd->matches += /*line->no_match =*/ (matches > 0);
            line->no_match = !matches; //(!matches && !vFlag) || (matches && vFlag);
            if (_line_print) {
#ifndef NO_COLOR
                //if (_colorize && matches) {
                if (_colorize && ((matches && !vFlag) || (!matches && vFlag))) {
                    if (ENGINE_WHOLE_LINE_MATCH == ret) {
                        if (*colors[LINE_MATCH].value) {
                            ustring_prepend_string(ustr, colors[LINE_MATCH].value);
                            ustring_append_string_len(ustr, reset, reset_len);
                        }
                    } else {
                        if (*colors[SINGLE_MATCH].value) {
                            int32_t decalage;
                            slist_element_t *el;
                            UChar *before;
                            int32_t before_len;

                            decalage = 0;
                            before = colors[SINGLE_MATCH].value;
                            before_len = u_strlen(before);
                            for (el = intervals->head; el; el = el->next) {
                                FETCH_DATA(el->data, i, interval_t);

                                ustring_insert_len(ustr, i->lower_limit + decalage, before, before_len);
                                ustring_insert_len(ustr, i->upper_limit + decalage + before_len, reset, reset_len);
                                decalage += before_len + reset_len;
                            }
                        }
                    }
                }
#endif /* !NO_COLOR */
                //if ((!vFlag && matches) || (vFlag && !matches)) {
                if (!line->no_match) {
                    int i;
                    flist_element_t *el;

                    if ( (before_context || after_context) && last_line_print > before_context && (fd->lineno - before_context > last_line_print + after_context) ) {
                        const UChar linesep[] = {SEP_NO_MATCH_UCHAR, SEP_NO_MATCH_UCHAR, U_NUL};

#ifndef NO_COLOR
                        if (colorize && *colors[CONTEXT_SEP].value) {
                            u_file_write(colors[CONTEXT_SEP].value, -1, ustdout);
                        }
#endif /* !NO_COLOR */
                        u_fputs(linesep, ustdout);
#ifndef NO_COLOR
                        if (colorize && *colors[CONTEXT_SEP].value) {
                            u_file_write(reset, reset_len, ustdout);
                        }
#endif /* !NO_COLOR */
                    }
                    fixed_circular_list_foreach(i, lines, el) {
                        //FETCH_DATA(el->data, ustr, UString);
                        FETCH_DATA(el->data, l, line_t);

                        if (fd->lineno - i > last_line_print) {
                            if (file_print) {
                                print_file(fd->filename, FALSE, l->no_match /*vFlag ? l->no_match : !l->no_match*/, TRUE, FALSE);
                            }
                            if (nFlag) {
                                print_line(fd->lineno - i, l->no_match /*vFlag ? l->no_match : !l->no_match*/, TRUE, FALSE);
                            }
                            u_fputs(l->ustr->ptr, ustdout);
                        }
                    }
                    last_line_print = fd->lineno;
                    _after_context = after_context;
                    fixed_circular_list_clean(lines);
                } else {
                    if (fd->lineno > last_line_print && _after_context > 0) {
                        if (file_print) {
                            print_file(fd->filename, FALSE, line->no_match /*vFlag ? matches : !matches*/, TRUE, FALSE);
                        }
                        if (nFlag) {
                            print_line(fd->lineno, line->no_match /*vFlag ? matches : !matches*/, TRUE, FALSE);
                        }
                        u_fputs(ustr->ptr, ustdout);
                        last_line_print = fd->lineno;
                        _after_context--;
                    }
                }
            }
#if 0
                if (matches > 0) {
                    if (!vFlag) {
                        /*if (file_print) {
                            print_file(fd->filename, FALSE, TRUE, FALSE);
                        }
                        if (nFlag) {
                            print_line(fd->lineno, TRUE, FALSE);
                        }*/
                        if (colorize) {
                            if (ENGINE_WHOLE_LINE_MATCH == ret) {
                                if (*colors[LINE_MATCH].value) {
                                    ustring_prepend_string(ustr, colors[LINE_MATCH].value);
                                    ustring_append_string_len(ustr, reset, reset_len);
                                }
                            } else {
                                if (*colors[SINGLE_MATCH].value) {
                                    int32_t decalage;
                                    slist_element_t *el;
                                    UChar *before;
                                    int32_t before_len;

                                    decalage = 0;
                                    before = colors[SINGLE_MATCH].value;
                                    before_len = u_strlen(before);
                                    for (el = intervals->head; el; el = el->next) {
                                        FETCH_DATA(el->data, i, interval_t);

                                        ustring_insert_len(ustr, i->lower_limit + decalage, before, before_len);
                                        ustring_insert_len(ustr, i->upper_limit + decalage + before_len, reset, reset_len);
                                        decalage += before_len + reset_len;
                                    }
                                }
                            }
                        }
                        //u_fputs(ustr->ptr, ustdout);
                        {
                            flist_element_t *el;

                            fixed_circular_list_foreach(lines, el) {
                                //FETCH_DATA(el->data, ustr, UString);
                                FETCH_DATA(el->data, l, line_t);

                                if (file_print) {
                                    print_file(fd->filename, FALSE, !l->no_match, TRUE, FALSE);
                                }
                                if (nFlag) {
                                    print_line(fd->lineno - _i, !l->no_match, TRUE, FALSE);
                                }
                                u_fputs(l->ustr->ptr, ustdout);
                            }
                            fixed_circular_list_clean(lines);
                        }
                    }
                } else {
                    if (fd->binary && BIN_FILE_BIN == binbehave) {
                        goto endfile; // no need to continue (file level)
                    } else if (vFlag) {
                        /*if (file_print) {
                            print_file(fd->filename, FALSE, FALSE, TRUE, FALSE);
                        }
                        if (nFlag) {
                            print_line(fd->lineno, FALSE, TRUE, FALSE);
                        }
                        u_fputs(ustr->ptr, ustdout);*/
                        {
                            flist_element_t *el;

                            fixed_circular_list_foreach(lines, el) {
                                //FETCH_DATA(el->data, ustr, UString);
                                FETCH_DATA(el->data, l, line_t);

                                if (file_print) {
                                    print_file(fd->filename, FALSE, l->no_match, TRUE, FALSE);
                                }
                                if (nFlag) {
                                    print_line(fd->lineno - _i, l->no_match, TRUE, FALSE);
                                }
                                u_fputs(l->ustr->ptr, ustdout);
                            }
                            fixed_circular_list_clean(lines);
                        }
                    }
                }
            }
#endif
        }
endfile:
        if (!_line_print) {
            if (cFlag) {
                if (file_print) {
                    print_file(fd->filename, fd->matches == 0, FALSE, TRUE, FALSE);
                }
                u_fprintf(ustdout, "%d\n", fd->matches);
            } else if (lFlag && fd->matches) {
                print_file(fd->filename, FALSE, FALSE, FALSE, TRUE);
            } else if (LFlag && !fd->matches) {
                print_file(fd->filename, TRUE, FALSE, FALSE, TRUE);
            } else if (fd->binary && BIN_FILE_BIN == binbehave && fd->matches/*((!vFlag && fd->matches) || (vFlag && !fd->matches))*/) {
                u_fprintf(ustdout, "Binary file %s matches\n", fd->filename);
            }
        }
        fd_close(fd);
    } else {
        print_error(error);
    }

    return fd->matches;
}

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
                matches += procfile(fd, p->fts_path);
                break;
        }
    }
    fts_close(fts);

    return matches;
}

/* ========== main ========== */

static void exit_cb(void)
{
    /*if (NULL != ustr) {
        ustring_destroy(ustr);
    }*/
    if (NULL != lines) {
        fixed_circular_list_destroy(lines);
    }
    if (NULL != patterns) {
        slist_destroy(patterns);
    }
    if (NULL != intervals) {
#ifdef OLD_INTERVAL
        slist_destroy(intervals);
#else
        slist_pool_destroy(intervals);
#endif /* OLD_INTERVAL */
    }
}

int main(int argc, char **argv)
{
#ifndef NO_COLOR
    enum {
        COLOR_AUTO,
        COLOR_NEVER,
        COLOR_ALWAYS
    };
#endif /* !NO_COLOR */

    int c;
    fd_t fd = { 0, 0, 0, 0, 0, 0, 0, 0 };
#ifndef NO_COLOR
    int color;
#endif /* !NO_COLOR */
    int matches;
    UBool iFlag;
    UBool wFlag;
    uint32_t flags;
    error_t *error;
    int pattern_type; // -E/F

    matches = 0;
    error = NULL;
    wFlag = FALSE;
    iFlag = FALSE;
#ifndef NO_COLOR
    color = COLOR_AUTO;
#endif /* !NO_COLOR */
    pattern_type = PATTERN_AUTO;

    if (0 != atexit(exit_cb)) {
        fputs("can't register atexit() callback", stderr);
        return UGREP_EXIT_FAILURE;
    }

    patterns = slist_new(pattern_destroy);
    default_reader = &mm_reader;
    ustdout = u_finit(stdout, NULL, NULL);
    ustderr = u_finit(stderr, NULL, NULL);

    debug("system locale = " YELLOW("%s"), u_fgetlocale(ustdout));
    debug("system codepage = " YELLOW("%s"), u_fgetcodepage(ustdout));

    // TODO: __progname (platform specific) => basename(argv[0])
    switch (__progname[1]) {
        case 'e':
            pattern_type = PATTERN_REGEXP;
            break;
        case 'f':
            pattern_type = PATTERN_LITERAL;
            break;
    }

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'A':
            case 'B':
            case 'C':
                if (NULL != optarg) {
                    long val;
                    char *endptr;

                    val = strtol(optarg, &endptr, 10);
                    if (0 != errno || endptr == optarg || *endptr != '\0' || val </*=*/ 0) {
                        fprintf(stderr, "Context out of range\n");
                        return UGREP_EXIT_USAGE;
                    }
                    if (c != 'A') {
                        after_context = val;
                    }
                    if (c != 'B') {
                        before_context = val;
                    }
                }
                break;
            case 'E':
                pattern_type = PATTERN_REGEXP;
                break;
            case 'F':
                pattern_type = PATTERN_LITERAL;
                break;
            case 'H':
                file_print = TRUE;
                break;
            case 'L':
                LFlag = TRUE;
                break;
            case 'R':
                rFlag = TRUE;
                break;
            case 'V':
                fprintf(stderr, "ugrep version %u.%u\n", UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
            case 'c':
                cFlag = TRUE;
                break;
            case 'e':
                OPTIONS_TO_ENGINE_FLAGS(flags, iFlag, wFlag, xFlag);
                if (!add_patternC(&error, patterns, optarg, pattern_type, flags)) {
                    print_error(error);
                }
                break;
            case 'f':
                OPTIONS_TO_ENGINE_FLAGS(flags, iFlag, wFlag, xFlag);
                if (!source_patterns(&error, optarg, patterns, pattern_type, flags)) {
                    print_error(error);
                }
                break;
            case 'h':
                file_print = FALSE;
                break;
            case 'i':
                iFlag = TRUE;
                break;
            case 'l':
                lFlag = TRUE;
                break;
            case 'q':
                file_print = line_print = FALSE;
                break;
            case 'n':
                nFlag = TRUE;
                break;
            case 'r':
                rFlag = TRUE;
                break;
            case 's':
                verbosity = FATAL;
                break;
            case 'v':
                vFlag = TRUE;
                break;
            case 'w':
                wFlag = TRUE;
                break;
            case 'x':
                xFlag = TRUE;
                break;
#ifndef NO_COLOR
            case COLOR_OPT:
                if (!strcmp("never", optarg)) {
                    color = COLOR_NEVER;
                } else if (!strcmp("auto", optarg)) {
                    color = COLOR_AUTO;
                } else if (!strcmp("always", optarg)) {
                    color = COLOR_ALWAYS;
                } else {
                    fprintf(stderr, "Unknown colo(u)r option\n");
                    return UGREP_EXIT_USAGE;
                }
                break;
#endif /* !NO_COLOR */
            case BINARY_OPT:
                if (!strcmp("binary", optarg)) {
                    binbehave = BIN_FILE_BIN;
                } else if (!strcmp("without-match", optarg)) {
                    binbehave = BIN_FILE_SKIP;
                } else if (!strcmp("text", optarg)) {
                    binbehave = BIN_FILE_TEXT;
                } else {
                    fprintf(stderr, "Unknown binary-files option\n");
                    return UGREP_EXIT_USAGE;
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
                        return UGREP_EXIT_USAGE;
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

    //line_print = line_print && !cFlag && !lFlag && !LFlag;
    if (cFlag || lFlag || LFlag) {
        before_context = after_context = 0;
        line_print = FALSE;
    }
    /* Options overrides, in case of incompatibility between them */
#ifndef NO_COLOR
    colorize = (COLOR_ALWAYS == color) || (COLOR_AUTO == color && stdout_is_tty());
#endif /* !NO_COLOR */
    if (binbehave != BIN_FILE_TEXT && (cFlag || lFlag || LFlag)) {
        binbehave = BIN_FILE_TEXT;
    }

    OPTIONS_TO_ENGINE_FLAGS(flags, iFlag, wFlag, xFlag);
    if (slist_empty(patterns)) {
        if (argc < 1) {
            usage();
        } else {
            if (!add_patternC(&error, patterns, *argv++, pattern_type, flags)) {
                print_error(error);
            }
            argc--;
        }
    }

#ifndef NO_COLOR
    if (colorize) {
        parse_userpref();
    }
#endif /* !NO_COLOR */

    //ustr = ustring_new();
    //lines = fixed_circular_list_new(before_context + 1, (func_ctor_t) ustring_new, (func_dtor_t) ustring_destroy);
    lines = fixed_circular_list_new(before_context + 1, line_ctor, line_dtor);
    intervals = intervals_new();

    if (0 == argc) {
        matches = procfile(&fd, "-");
    } else if (rFlag) {
        matches = procdir(&fd, argv);
    } else {
        for ( ; argc--; ++argv) {
            matches += procfile(&fd, *argv);
        }
    }

    return (matches > 0 ? UGREP_EXIT_MATCH : UGREP_EXIT_NO_MATCH);
}
