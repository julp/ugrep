#include <limits.h>
#ifdef _MSC_VER
# include <shlobj.h>
#else
# include <sys/param.h>
# include <pwd.h>
#endif /* _MSC_VER */
#ifdef WITH_FTS
# include <fts.h>
#endif /* WITH_FTS */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#include "engine.h"
#include "parsenum.h"
#include "struct/fixed_circular_list.h"

#define SEP_MATCH_UCHAR    0x003a
#define SEP_NO_MATCH_UCHAR 0x002d


#define UGREP_EXIT_SUCCESS UGREP_EXIT_NO_MATCH
enum {
    UGREP_EXIT_MATCH    = 0,
    UGREP_EXIT_NO_MATCH = 1,
    UGREP_EXIT_FAILURE  = 2,
    UGREP_EXIT_USAGE
};

enum {
    PATTERN_LITERAL,
    PATTERN_REGEXP,
    PATTERN_AUTO
};

typedef struct {
    UString *ustr;
    UBool match;
#ifndef NO_COLOR
# ifdef _MSC_VER
    interval_list_t *intervals;
    engine_return_t ret;
    int pattern_matches;
# endif /* _MSC_VER */
#endif /* !NO_COLOR */
} line_t;

void *line_ctor(void) {
    line_t *l;

    l = mem_new(*l);
    l->ustr = ustring_new();
#ifndef NO_COLOR
# ifdef _MSC_VER
    l->intervals = interval_list_new();
# endif /* _MSC_VER */
#endif /* !NO_COLOR */

    return l;
}

void line_dtor(void *data) {
    FETCH_DATA(data, l, line_t);

    ustring_destroy(l->ustr);
#ifndef NO_COLOR
# ifdef _MSC_VER
    interval_list_destroy(l->intervals);
# endif /* _MSC_VER */
#endif /* !NO_COLOR */
    free(l);
}

#ifdef _MSC_VER
void line_clean(void *data) {
    FETCH_DATA(data, l, line_t);

    interval_list_clean(l->intervals);
}
#endif /* _MSC_VER */

/* ========== global variables ========== */

extern engine_t fixed_engine;
// extern engine_t bin_engine;
extern engine_t re_engine;

engine_t *engines[] = {
    &fixed_engine,
//     &bin_engine,
    &re_engine
};


static fixed_circular_list_t *lines = NULL;
static slist_t *patterns = NULL;
static int binbehave = BIN_FILE_SKIP;
#ifndef NO_COLOR
# ifndef _MSC_VER
static interval_list_t *intervals = NULL;
# endif /* !_MSC_VER */
#endif /* !NO_COLOR */

static UBool oFlag = FALSE;
static UBool xFlag = FALSE;
static UBool nFlag = FALSE;
static UBool vFlag = FALSE;
static UBool cFlag = FALSE;
static UBool lFlag = FALSE;
static UBool LFlag = FALSE;

static uint32_t after_context = 0;
static uint32_t before_context = 0;
static uint32_t max_count = UINT32_MAX;

static UBool file_print = FALSE; // -H/h
#ifndef NO_COLOR
static UBool colorize = TRUE;
#endif /* !NO_COLOR */
static UBool line_print = TRUE;

static int return_values[2/*error?*/][2/*matches?*/] = {
    { UGREP_EXIT_FAILURE, UGREP_EXIT_MATCH },
    { UGREP_EXIT_NO_MATCH, UGREP_EXIT_MATCH }
};

/* ========== general helper functions ========== */

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
        0x002e, // .
        0
    };

    return (NULL != u_strpbrk(pattern, meta));
}

static UBool is_patternC(const char *pattern)
{
    return (NULL != strpbrk(pattern, "\\+*?[^]$(){}=!<>|:-."));
}

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC,
// #ifndef NO_COLOR
    COLOR_OPT,
// #endif /* !NO_COLOR */
    //READER_OPT
};

static char optstr[] = "0123456789A:B:C:EFHLVce:f:hilm:noqsvwx" FTS_COMMON_OPTIONS_STRING;

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
#ifdef WITH_FTS
    FTS_COMMON_OPTIONS,
#endif /* WITH_FTS */
// #ifndef NO_COLOR
    {"color",               required_argument, NULL, COLOR_OPT},
    {"colour",              required_argument, NULL, COLOR_OPT},
// #endif /* !NO_COLOR */
    {"binary-files",        required_argument, NULL, BINARY_OPT},
    {"after-context",       required_argument, NULL, 'A'},
    {"before-context",      required_argument, NULL, 'B'},
    {"context",             required_argument, NULL, 'C'},
    {"extended-regexp",     no_argument,       NULL, 'E'}, // POSIX
    {"fixed-string",        no_argument,       NULL, 'F'}, // POSIX
    {"with-filename",       no_argument,       NULL, 'H'},
    {"files-without-match", no_argument,       NULL, 'L'},
    {"version",             no_argument,       NULL, 'V'},
    {"count",               no_argument,       NULL, 'c'}, // POSIX
    {"regexp",              required_argument, NULL, 'e'}, // POSIX
    {"file",                required_argument, NULL, 'f'}, // POSIX
    {"no-filename",         no_argument,       NULL, 'h'},
    {"ignore-case",         no_argument,       NULL, 'i'}, // POSIX
    {"files-with-matches",  no_argument,       NULL, 'l'},
    {"max-count",           required_argument, NULL, 'm'},
    {"line-number",         no_argument,       NULL, 'n'}, // POSIX
    {"only-match",          no_argument,       NULL, 'o'},
    {"quiet",               no_argument,       NULL, 'q'}, // POSIX
    {"silent",              no_argument,       NULL, 'q'}, // POSIX
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
        "usage: %s [-0123456789EFHLRVchilnoqrsvwx] [-A num] [-B num]\n"
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

UBool add_pattern(error_t **error, slist_t *l, UString *ustr, int pattern_type, uint32_t flags)
{
    void *data;
    pattern_data_t *pdata;

    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_pattern(ustr->ptr) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
//     if (PATTERN_LITERAL == pattern_type) {
        ustring_unescape(ustr);
//     }
    if (ustring_empty(ustr)) {
        pattern_type = PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compile(error, ustr, flags))) {
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
    UString *ustr;
    pattern_data_t *pdata;

    if (PATTERN_AUTO == pattern_type) {
        pattern_type = is_patternC(pattern) ? PATTERN_REGEXP : PATTERN_LITERAL;
    }
    if (NULL == (ustr = ustring_convert_argv_from_local(pattern, error, TRUE/*PATTERN_LITERAL == pattern_type*/))) {
        return FALSE;
    }
    if (ustring_empty(ustr)) {
        pattern_type = PATTERN_LITERAL;
    }
    if (NULL == (data = engines[!!pattern_type]->compile(error, ustr, flags))) {
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
    reader_t reader;
    UBool retval;
    UString *ustr;

    retval = TRUE;
    reader_set_imp_by_name(&reader, "stdio");
    if (!reader_open(&reader, error, filename)) {
        return FALSE;
    }
    ustr = ustring_new();
    while (retval && !reader_eof(&reader)) {
        if (!reader_readline(&reader, error, ustr)) {
            retval = FALSE;
        } else {
            ustring_chomp(ustr);
            if (!add_pattern(error, l, ustr, pattern_type, flags)) {
                retval = FALSE;
            }
        }
    }
    reader_close(&reader);
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

# ifdef _MSC_VER

#  include <Windows.h>
#  include <Wincon.h>

typedef struct {
    const char *name;
    WORD fg;
    WORD bg;
} attr_t;

attr_t attrs[] = {
    {"black",        0,                                                                    0},
    {"red",          FOREGROUND_RED,                                                       BACKGROUND_RED},
    {"lightred",     FOREGROUND_RED|FOREGROUND_INTENSITY,                                  BACKGROUND_RED|BACKGROUND_INTENSITY},
    {"green",        FOREGROUND_GREEN,                                                     BACKGROUND_GREEN},
    {"lightgreen",   FOREGROUND_GREEN|FOREGROUND_INTENSITY,                                BACKGROUND_GREEN|BACKGROUND_INTENSITY},
    {"yellow",       FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,                 BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_INTENSITY},
    {"blue",         FOREGROUND_BLUE,                                                      BACKGROUND_BLUE},
    {"lightblue",    FOREGROUND_BLUE|FOREGROUND_INTENSITY,                                 BACKGROUND_BLUE|BACKGROUND_INTENSITY},
    {"magenta",      FOREGROUND_RED|FOREGROUND_BLUE,                                       BACKGROUND_RED|BACKGROUND_BLUE},
    {"lightmagenta", FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY,                  BACKGROUND_RED|BACKGROUND_BLUE|BACKGROUND_INTENSITY},
    {"cyan",         FOREGROUND_GREEN|FOREGROUND_BLUE,                                     BACKGROUND_GREEN|BACKGROUND_BLUE},
    {"lightcyan",    FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY,                BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY},
    {"white",        FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY, BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY},
    {"default",      FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,                      0},
    {"brown",        FOREGROUND_RED|FOREGROUND_GREEN,                                      BACKGROUND_RED|BACKGROUND_GREEN},
    {"gray",         FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,                      BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE},
    {NULL,           -1,                                                                   -1}
};

typedef struct {
    const char *name;
    WORD value;
} color_t;

color_t colors[] = {
    {"single-match",  FOREGROUND_BLUE},
    {"line-match",    FOREGROUND_BLUE|FOREGROUND_INTENSITY},
    {"file-match",    FOREGROUND_GREEN},
    {"file-no-match", FOREGROUND_RED},
    {"line-number",   FOREGROUND_RED|FOREGROUND_BLUE},
    {"sep-match",     FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY},
    {"sep-no-match",  FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY},
    {"context-sep",   FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY},
    {NULL,            0}
};

static WORD reset = 0;

# else

typedef struct {
    const char *name;
    int fg;
    int bg;
} attr_t;

static const UChar reset[] = {0x001b, 0x005b, 0x0030, 0x006d, 0};
static const int32_t reset_len = ARRAY_SIZE(reset) - 1;

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

# define MAX_ATTRS   8
# define MAX_SEQ_LEN 64

typedef struct {
    const char *name;
    UChar value[MAX_SEQ_LEN];
} color_t;

color_t colors[] = {
    {"single-match",  {0x001b, 0x005b, 0x0031, 0x003b, 0x0033, 0x0034, 0x006d, 0}},
    {"line-match",    {0x001b, 0x005b, 0x0031, 0x003b, 0x0033, 0x0036, 0x006d, 0}},
    {"file-match",    {0x001b, 0x005b, 0x0033, 0x0032, 0x006d, 0}},
    {"file-no-match", {0x001b, 0x005b, 0x0033, 0x0031, 0x006d, 0}},
    {"line-number",   {0x001b, 0x005b, 0x0033, 0x0035, 0x006d, 0}},
    {"sep-match",     {0x001b, 0x005b, 0x0033, 0x0033, 0x006d, 0}},
    {"sep-no-match",  {0x001b, 0x005b, 0x0033, 0x0033, 0x006d, 0}},
    {"context-sep",   {0x001b, 0x005b, 0x0033, 0x0033, 0x006d, 0}},
    {NULL,            {0}}
};

static UChar *u_stpncpy(UChar *dest, const UChar *src, int32_t length)
{
    int32_t n = u_strlen(src);

    if (n > length) {
        return NULL;
    }

    return u_strncpy(dest, src, length) + n;
}

# endif /* _MSC_VER */

void console_apply_color(color_type_t c)
{
# ifdef _MSC_VER
    if (colorize && colors[c].value) { // TODO: black could not be used!
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colors[c].value);
# else
    if (colorize && *colors[c].value) {
        u_file_write(colors[c].value, -1, ustdout);
# endif /* _MSC_VER */
    }
}

void console_reset(color_type_t c)
{
# ifdef _MSC_VER
    if (colorize && colors[c].value) { // TODO: black could not be used!
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), reset);
# else
    if (colorize && *colors[c].value) {
        u_file_write(reset, reset_len, ustdout);
# endif /* _MSC_VER */
    }
}

static void parse_userpref(void)
{
    char *home;
    char buffer[MAXPATHLEN];

    buffer[0] = '\0';
    if (NULL == (home = getenv("HOME"))) {
# ifdef _MSC_VER
#  ifndef CSIDL_PROFILE
#   define CSIDL_PROFILE 40
#  endif /* CSIDL_PROFILE */
        if (NULL == (home = getenv("USERPROFILE"))) {
            HRESULT hr;
            LPITEMIDLIST pidl = NULL;

            hr = SHGetSpecialFolderLocation(NULL, CSIDL_PROFILE, &pidl);
            if (S_OK == hr) {
                SHGetPathFromIDList(pidl, buffer);
                home = buffer;
                CoTaskMemFree(pidl);
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

        if (snprintf(preffile, ARRAY_SIZE(preffile), "%s%c%s", home, DIRECTORY_SEPARATOR, ".ugrep") < (int) ARRAY_SIZE(preffile)) {
            struct stat st;

            if ((-1 != (stat(preffile, &st))) && S_ISREG(st.st_mode)) {
                FILE *fp;

                if (NULL != (fp = fopen(preffile, "r"))) {
                    char line[4096];

                    while (NULL != fgets(line, ARRAY_SIZE(line), fp)) {
                        color_t *c;

                        if ('\n' == *line || '#' == *line) {
                            continue;
                        }
                        for (c = colors; c->name; c++) {
                            if (!strncmp(c->name, line, strlen(c->name))) {
                                char *s, *t;
                                int colors_count;
# ifdef _MSC_VER
                                WORD user_attrs;
# else
                                int attrs_count, user_attrs[MAX_ATTRS];
# endif /* _MSC_VER */

                                s = line + strlen(c->name);
# ifdef _MSC_VER
                                user_attrs = colors_count = 0;
# else
                                attrs_count = colors_count = 0;
# endif /* _MSC_VER */
                                do {
                                    while (isblank(*s) || ispunct(*s)) {
                                        s++;
                                    }
                                    t = s;
                                    if (islower(*s)) {
                                        while (islower(*++s))
                                            ;
                                        if (s > t) {
                                            attr_t *a;

                                            *s++ = 0; // TODO: is it safe?
                                            if (!strcmp("none", t)) {
# ifdef _MSC_VER
                                                c->value = 0;
# else
                                                *c->value = 0;
# endif /* _MSC_VER */
                                                break;
                                            } else {
                                                for (a = attrs; NULL != a->name; a++) {
                                                    if (!strcmp(a->name, t)) {
                                                        if (a->bg == a->fg || (a->bg != a->fg && colors_count < 2)) {
# ifdef _MSC_VER
                                                            user_attrs |= (colors_count++ ? a->bg : a->fg);
# else
                                                            user_attrs[attrs_count++] = colors_count++ ? a->bg : a->fg;
# endif /* _MSC_VER */
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
# ifndef _MSC_VER
                                    else if (isdigit(*s)) {
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

                                                for (a = attrs; NULL != a->name; a++) {
                                                    if (val == a->bg || val == a->fg) {
                                                        if (a->bg == a->fg || (a->bg != a->fg && colors_count++ < 2)) {
                                                            user_attrs[attrs_count++] = val;
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
# endif /* !_MSC_VER */
                                } while (*s && '\n' != *s);
# ifdef _MSC_VER
                                c->value = user_attrs;
# else
                                if (attrs_count > 0) {
                                    UChar prefix[] = {0x001b, 0x005b, 0}, suffix[] = {0x006d, 0}, sep[] = {0x003b, 0};
                                    UChar *ptr, buf[MAX_SEQ_LEN], defval[MAX_SEQ_LEN];
                                    UFILE *ufp;
                                    int i, len;

                                    ptr = c->value;
                                    u_strcpy(defval, c->value);
                                    if (NULL != (ufp = u_fstropen(buf, ARRAY_SIZE(buf), NULL))) {
                                        if (NULL == (ptr = u_stpncpy(c->value, prefix, MAX_SEQ_LEN - (ptr - c->value)))) {
                                            u_strcpy(c->value, defval); // restore default color
                                            u_fclose(ufp);
                                            goto nextline;
                                        }
                                        for (i = 0; i < attrs_count; i++) {
                                            len = u_fprintf(ufp, "%d", user_attrs[i]);
                                            buf[len] = 0;
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
# endif /* _MSC_VER */
                            }
                        }
# ifndef _MSC_VER
nextline:
                        ;
# endif /* !_MSC_VER */
                    }
                }
            }
        }
    }
}

#endif /* !NO_COLOR */

/* ========== process on file and/or directory helper ========== */

static void print_file(const char *filename, UBool no_file_match, UBool line_match, UBool print_sep, UBool eol)
{
#ifndef NO_COLOR
    console_apply_color(no_file_match ? FILE_NO_MATCH : FILE_MATCH);
#endif /* !NO_COLOR */
    u_fprintf(ustdout, "%s", filename);
#ifndef NO_COLOR
    console_reset(no_file_match ? FILE_NO_MATCH : FILE_MATCH);
#endif /* !NO_COLOR */
    if (print_sep) {
#ifndef NO_COLOR
        console_apply_color(line_match ? SEP_MATCH : SEP_NO_MATCH);
#endif /* !NO_COLOR */
        u_fputc(line_match ? SEP_MATCH_UCHAR : SEP_NO_MATCH_UCHAR, ustdout);
#ifndef NO_COLOR
        console_reset(line_match ? SEP_MATCH : SEP_NO_MATCH);
#endif /* !NO_COLOR */
    }
    if (eol) {
        u_file_write(EOL, EOL_LEN, ustdout);
    }
}

static void print_line(int lineno, UBool line_match, UBool print_sep, UBool eol)
{
#ifndef NO_COLOR
    console_apply_color(LINE_NUMBER);
#endif /* !NO_COLOR */
    u_fprintf(ustdout, "%d", lineno);
#ifndef NO_COLOR
    console_reset(LINE_NUMBER);
#endif /* !NO_COLOR */
    if (print_sep) {
#ifndef NO_COLOR
        console_apply_color(line_match ? SEP_MATCH : SEP_NO_MATCH);
#endif /* !NO_COLOR */
        u_fputc(line_match ? SEP_MATCH_UCHAR : SEP_NO_MATCH_UCHAR, ustdout);
#ifndef NO_COLOR
        console_reset(line_match ? SEP_MATCH : SEP_NO_MATCH);
#endif /* !NO_COLOR */
    }
    if (eol) {
        u_file_write(EOL, EOL_LEN, ustdout);
    }
}

#ifdef DEBUG
# ifdef OLD_RING
#  define RING_ELEMENT_USED(e) e.used
# else
#  define RING_ELEMENT_USED(e) *e.used
# endif /* OLD_RING */
void fixed_circular_list_print(fixed_circular_list_t *l) /* NONNULL() */
{
    size_t i;

    require_else_return(NULL != l);

    u_fprintf(ustdout, "| i |   ADDR   | USED | PTR | HEAD | USTRING\n");
    u_fprintf(ustdout, "--------------------------------------------\n");
    for (i = 0; i < l->len; i++) {
        FETCH_DATA(l->elts[i].data, x, line_t);
        u_fprintf(ustdout, "| %d | %p | %4d | %3d | %4d | %S\n", i, &l->elts[i], RING_ELEMENT_USED(l->elts[i]), &l->elts[i] == l->ptr, &l->elts[i] == l->head, x->ustr->ptr);
    }
}
#endif /* DEBUG */

static int procfile(reader_t *reader, const char *filename, void *userdata)
{
    uint32_t *matches;
    UString *ustr;
    error_t *error;
#ifndef NO_COLOR
# ifdef _MSC_VER
    interval_list_t *intervals = NULL;
# endif /* _MSC_VER */
    UBool _colorize;
#endif /* !NO_COLOR */
    uint32_t last_line_print;
    uint32_t arg_matches; // matches (for the current file) against command arguments (-v)
    int _after_context;

    error = NULL;
    arg_matches = 0;
    _after_context = 0;
    last_line_print = 0;
    matches = (uint32_t *) userdata;
#ifndef NO_COLOR
    _colorize = colorize && (before_context || after_context || !vFlag);
#endif /* !NO_COLOR */

    fixed_circular_list_clean(lines);
    if (reader_open(reader, &error, filename)) {
        UBool _line_print; /* line_print local override */

        _line_print = line_print && (!reader->binary || (reader->binary && BIN_FILE_BIN != binbehave));
        while (!reader_eof(reader)) {
            int pattern_matches; // matches (for the current line) against pattern(s), doesn't take care of arguments (-v)
            slist_element_t *p;
            engine_return_t ret;
            FETCH_DATA(fixed_circular_list_fetch(lines), line, line_t);

            ustr = line->ustr;
            ret = ENGINE_FAILURE;
            if (!reader_readline(reader, &error, ustr)/* && NULL != error*/) {
                print_error(error);
            }
            pattern_matches = 0;
            ustring_chomp(ustr);
            if (BIN_FILE_TEXT == binbehave) {
                ustring_dump(ustr);
            }
#ifndef NO_COLOR
# ifdef _MSC_VER
            intervals = line->intervals;
# else
            interval_list_clean(intervals);
# endif /* _MSC_VER */
#endif /* !NO_COLOR */
            for (p = patterns->head; NULL != p; p = p->next) {
                FETCH_DATA(p->data, pdata, pattern_data_t);

                if (xFlag) {
                    ret = pdata->engine->whole_line_match(&error, pdata->pattern, ustr);
                } else {
#ifndef NO_COLOR
                    if (oFlag || (_colorize && _line_print)) {
                        ret = pdata->engine->match_all(&error, pdata->pattern, ustr, intervals);
                    } else {
#endif /* !NO_COLOR */
                        ret = pdata->engine->match(&error, pdata->pattern, ustr);
#ifndef NO_COLOR
                    }
#endif /* !NO_COLOR */
                }
                if (ENGINE_FAILURE == ret) {
                    reader_close(reader);
                    print_error(error);
                    return 1;
                } else if (ENGINE_WHOLE_LINE_MATCH == ret) {
                    pattern_matches++;
                    break; // no need to continue (line level)
                } else {
                    pattern_matches += ret;
                }
                //if (pattern_matches > 0 && (lFlag || (!vFlag && fd->binary && BIN_FILE_BIN == binbehave))) {
                if (pattern_matches && (lFlag || (reader->binary && BIN_FILE_BIN == binbehave))) {
                    debug("file skipping (%s)", reader->sourcename);
                    arg_matches = 1;
                    goto endfile; // no need to continue (file level)
                }
            }
            if (!vFlag) {
                line->match = !!pattern_matches;
            } else {
                line->match = !pattern_matches;
            }
            arg_matches += line->match;
            if (_line_print) {
#ifndef NO_COLOR
# ifndef _MSC_VER
                if (!oFlag && _colorize && pattern_matches) {
                    if (ENGINE_WHOLE_LINE_MATCH == ret) {
                        if (*colors[LINE_MATCH].value) {
                            ustring_prepend_string(ustr, colors[LINE_MATCH].value);
                            ustring_append_string_len(ustr, reset, reset_len);
                        }
                    } else {
                        if (!oFlag && *colors[SINGLE_MATCH].value) {
                            UChar *before;
                            int32_t decalage;
                            int32_t before_len;
                            dlist_element_t *el;

                            decalage = 0;
                            before = colors[SINGLE_MATCH].value;
                            before_len = u_strlen(before);
                            for (el = intervals->head; NULL != el; el = el->next) {
                                FETCH_DATA(el->data, i, interval_t);

                                ustring_insert_len(ustr, i->lower_limit + decalage, before, before_len);
                                ustring_insert_len(ustr, i->upper_limit + decalage + before_len, reset, reset_len);
                                decalage += before_len + reset_len;
                            }
                        }
                    }
                }
# else
                line->ret = ret;
                line->pattern_matches = pattern_matches;
# endif /* !_MSC_VER */
#endif /* !NO_COLOR */
                if (line->match) {
                    int i;
                    flist_element_t *el;

                    if ( (before_context || after_context) && last_line_print > before_context && (reader->lineno - before_context > last_line_print + 1) ) {
                        const UChar linesep[] = {SEP_NO_MATCH_UCHAR, SEP_NO_MATCH_UCHAR, 0};

#ifndef NO_COLOR
                        console_apply_color(CONTEXT_SEP);
#endif /* !NO_COLOR */
                        u_fputs(linesep, ustdout);
#ifndef NO_COLOR
                        console_reset(CONTEXT_SEP);
#endif /* !NO_COLOR */
                    }
#if 0
                    fixed_circular_list_print(lines);
#endif
                    fixed_circular_list_foreach(i, lines, el) {
                        FETCH_DATA(el->data, l, line_t);

                        if (reader->lineno - i > last_line_print) { // without this test, lines in both, after and before, contexts will be printed twice
                            if (file_print) {
                                print_file(reader->sourcename, FALSE, l->match, TRUE, FALSE);
                            }
                            if (nFlag) {
                                print_line(reader->lineno - i, l->match, TRUE, FALSE);
                            }
                            if (oFlag) {
                                dlist_element_t *e;

                                for (e = intervals->head; NULL != e; e = e->next) {
                                    FETCH_DATA(e->data, i, interval_t);

                                    console_apply_color(SINGLE_MATCH);
                                    u_file_write(l->ustr->ptr + i->lower_limit, i->upper_limit - i->lower_limit, ustdout);
                                    console_reset(SINGLE_MATCH);
                                    u_file_write(EOL, EOL_LEN, ustdout);
                                }
                            } else {
#if defined(NO_COLOR) || !defined(_MSC_VER)
                                u_fputs(l->ustr->ptr, ustdout);
#else
                                if (ENGINE_WHOLE_LINE_MATCH == l->ret && _colorize && colors[LINE_MATCH].value) {
                                    console_apply_color(LINE_MATCH);
                                    u_fputs(l->ustr->ptr, ustdout);
                                    console_reset(LINE_MATCH);
                                } else if (l->pattern_matches /* > 0 */ && _colorize && colors[SINGLE_MATCH].value) {
                                    int32_t last = 0;
                                    dlist_element_t *e;

                                    for (e = l->intervals->head; NULL != e; e = e->next) {
                                        FETCH_DATA(e->data, i, interval_t);

                                        if (last < i->lower_limit) {
                                            u_file_write(l->ustr->ptr + last, i->lower_limit - last, ustdout);
                                        }
                                        console_apply_color(SINGLE_MATCH);
                                        u_file_write(l->ustr->ptr + i->lower_limit, i->upper_limit - i->lower_limit, ustdout);
                                        console_reset(SINGLE_MATCH);
                                        last = i->upper_limit;
                                    }
                                    if (last < ustr->len) {
                                        u_file_write(l->ustr->ptr + last, ustr->len - last, ustdout);
                                    }
                                    u_file_write(EOL, EOL_LEN, ustdout);
                                } else {
                                    u_fputs(l->ustr->ptr, ustdout);
                                }
#endif /* NO_COLOR || !_MSC_VER */
                            }
                        }
                    }
                    last_line_print = reader->lineno;
                    _after_context = after_context;
                    fixed_circular_list_clean(lines);
                } else {
                    if (reader->lineno > last_line_print && _after_context > 0) {
                        if (file_print) {
                            print_file(reader->sourcename, FALSE, line->match, TRUE, FALSE);
                        }
                        if (nFlag) {
                            print_line(reader->lineno, line->match, TRUE, FALSE);
                        }
#if defined(NO_COLOR) || !defined(_MSC_VER)
                        u_fputs(ustr->ptr, ustdout);
#else
                        if (ENGINE_WHOLE_LINE_MATCH == line->ret && _colorize && colors[LINE_MATCH].value) {
                            console_apply_color(LINE_MATCH);
                            u_fputs(ustr->ptr, ustdout);
                            console_reset(LINE_MATCH);
                        } else if (line->pattern_matches /* > 0 */ && _colorize && colors[SINGLE_MATCH].value) {
                            int32_t last = 0;
                            dlist_element_t *e;

                            for (e = intervals->head; NULL != e; e = e->next) {
                                FETCH_DATA(e->data, i, interval_t);

                                if (last < i->lower_limit) {
                                    u_file_write(ustr->ptr + last, i->lower_limit - last, ustdout);
                                }
                                console_apply_color(SINGLE_MATCH);
                                u_file_write(ustr->ptr + i->lower_limit, i->upper_limit - i->lower_limit, ustdout);
                                console_reset(SINGLE_MATCH);
                                last = i->upper_limit;
                            }
                            if (last < ustr->len) {
                                u_file_write(ustr->ptr + last, ustr->len - last, ustdout);
                            }
                            u_file_write(EOL, EOL_LEN, ustdout);
                        } else {
                            u_fputs(ustr->ptr, ustdout);
                        }
#endif /* NO_COLOR || !_MSC_VER */
                        last_line_print = reader->lineno;
                        _after_context--;
                    }
                }
            }
            if (arg_matches >= max_count) {
                goto endfile;
            }
        }
endfile:
        if (!_line_print) {
            if (cFlag) {
                if (file_print) {
                    print_file(reader->sourcename, arg_matches == 0, TRUE, TRUE, FALSE);
                }
                u_fprintf(ustdout, "%d\n", arg_matches);
            } else if (lFlag && arg_matches) {
                print_file(reader->sourcename, FALSE, FALSE, FALSE, TRUE);
            } else if (LFlag && !arg_matches) {
                print_file(reader->sourcename, TRUE, FALSE, FALSE, TRUE);
            } else if (reader->binary && BIN_FILE_BIN == binbehave && arg_matches/*((!vFlag && fd->matches) || (vFlag && !fd->matches))*/) {
                u_fprintf(ustdout, "Binary file %s matches\n", reader->sourcename);
            }
        }
    } else {
        print_error(error);
        return 1;
    }
    reader_close(reader);
    *matches += arg_matches;

    return 0;
}

/* ========== main ========== */

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
    int ret;
    int lastc;
    UBool newarg;
    int prevoptind;
    reader_t *reader;
#ifndef NO_COLOR
    int color;
#endif /* !NO_COLOR */
    UBool iFlag;
    UBool wFlag;
    uint32_t flags;
    int strength;
    error_t *error;
    uint32_t matches;
    int pattern_type; // -E/F

    ret = 0;
    matches = 0;
    strength = 0;
    error = NULL;
    wFlag = FALSE;
    iFlag = FALSE;
#ifndef NO_COLOR
    color = COLOR_AUTO;
#endif /* !NO_COLOR */
    pattern_type = PATTERN_AUTO;

    env_init(UGREP_EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);
    patterns = slist_new(pattern_destroy);
    env_register_resource(patterns, (func_dtor_t) slist_destroy);

    switch (__progname[1]) {
        case 'e':
            pattern_type = PATTERN_REGEXP;
            break;
        case 'f':
            pattern_type = PATTERN_LITERAL;
            break;
    }

    lastc = '\0';
    newarg = TRUE;
    prevoptind = 1;
    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (newarg || !isdigit(lastc)) {
                    after_context = before_context = 0;
                }
                after_context = before_context = after_context * 10 + (c - '0');
                break;
            case 'A':
            case 'B':
            case 'C':
                if (NULL != optarg) {
                    int32_t min, val;

                    min = 0;
                    if (PARSE_NUM_NO_ERR != parse_int32_t(optarg, NULL, 10, &min, NULL, &val)) {
                        fprintf(stderr, "Invalid context '%s'\n", optarg);
                        return UGREP_EXIT_USAGE;
                    }
                    if ('A' != c) {
                        before_context = (uint32_t) val;
                    }
                    if ('B' != c) {
                        after_context = (uint32_t) val;
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
            case 'V':
                fprintf(stderr, "BSD %s version %u.%u\n" COPYRIGHT, __progname, UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                return UGREP_EXIT_SUCCESS;
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
                strength++;
                break;
            case 'l':
                lFlag = TRUE;
                break;
            case 'o':
                oFlag = TRUE;
                break;
            case 'q':
                file_print = line_print = FALSE;
                break;
            case 'm':
            {
                int32_t min, val;

                min = 0;
                if (PARSE_NUM_NO_ERR != parse_int32_t(optarg, NULL, 10, &min, NULL, &val)) {
                    fprintf(stderr, "Invalid limit '%s'\n", optarg);
                    return UGREP_EXIT_USAGE;
                }
                max_count = (uint32_t) val;
                break;
            }
            case 'n':
                nFlag = TRUE;
                break;
            case 's':
                env_set_verbosity(FATAL);
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
            case COLOR_OPT:
#ifndef NO_COLOR
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
#endif /* !NO_COLOR */
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
                    return UGREP_EXIT_USAGE;
                }
                break;
            default:
                if (!util_opt_parse(c, optarg, reader)) {
                    usage();
                }
                break;
        }
        lastc = c;
        newarg = optind != prevoptind;
        prevoptind = optind;
    }
    argc -= optind;
    argv += optind;

    env_apply();

    reader_set_binary_behavior(reader, binbehave);

    /* Options overrides, in case of incompatibility between them */
    if (cFlag || lFlag || LFlag) {
        before_context = after_context = 0;
        line_print = FALSE;
    }
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
            if (!add_patternC(&error, patterns, *argv++, pattern_type, strength | flags)) {
                print_error(error);
            }
            argc--;
        }
    }

#ifndef NO_COLOR
    if (colorize) {
# ifdef _MSC_VER
        CONSOLE_SCREEN_BUFFER_INFO info;

        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
        reset = info.wAttributes;
# endif /* _MSC_VER */
        parse_userpref();
    }
#endif /* !NO_COLOR */

#ifdef OLD_RING
# ifdef _MSC_VER
    lines = fixed_circular_list_new(before_context + 1, line_ctor, line_dtor, line_clean);
# else
    lines = fixed_circular_list_new(before_context + 1, line_ctor, line_dtor, NULL);
# endif /* _MSC_VER */
#else
    lines = fixed_circular_list_new(before_context + 1, line_ctor, line_dtor);
#endif /* OLD_RING */
    env_register_resource(lines, (func_dtor_t) fixed_circular_list_destroy);
#ifndef NO_COLOR
# ifndef _MSC_VER
    intervals = interval_list_new();
    env_register_resource(intervals, (func_dtor_t) interval_list_destroy);
# endif /* !_MSC_VER */
#endif /* !NO_COLOR */

    if (0 == argc) {
        ret |= procfile(reader, "-", &matches);
#ifdef WITH_FTS
    } else if (DIR_RECURSE == get_dirbehave()) {
        ret |= procdir(reader, argv, &matches, procfile);
#endif /* WITH_FTS */
    } else {
        for ( ; argc--; ++argv) {
#ifdef WITH_FTS
            if (!is_file_matching(*argv)) {
                continue;
            }
#endif /* WITH_FTS */
            ret |= procfile(reader, *argv, &matches);
        }
    }

    return return_values[0 == ret][matches > 0];
}
