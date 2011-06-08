#include <limits.h>
#ifdef _MSC_VER
# include <shlobj.h>
#else
# include <sys/param.h>
# include <pwd.h>
#endif /* _MSC_VER */
#ifndef WITHOUT_FTS
# include <fts.h>
#endif /* !WITHOUT_FTS */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#include "ugrep.h"


#define SEP_MATCH_UCHAR    0x003a
#define SEP_NO_MATCH_UCHAR 0x002d

// const UChar EOL[] = {U_CR, U_LF, U_NUL};
// const UChar EOL[] = {U_LF, U_NUL};
// const size_t EOL_LEN = ARRAY_SIZE(EOL) - 1;


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

extern engine_t fixed_engine;
extern engine_t re_engine;

engine_t *engines[] = {
    &fixed_engine,
    &re_engine
};


static fixed_circular_list_t *lines = NULL;
static slist_t *patterns = NULL;
int binbehave = BIN_FILE_SKIP;
#ifdef OLD_INTERVAL
static slist_t *intervals = NULL;
#else
static slist_pool_t *intervals = NULL;
#endif /* OLD_INTERVAL */

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

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = CHAR_MAX + 1,
    INPUT_OPT,
#ifndef NO_COLOR
    COLOR_OPT,
#endif /* !NO_COLOR */
    READER_OPT
};

#ifndef WITHOUT_FTS
static char optstr[] = "0123456789A:B:C:EFHLRVce:f:hilnqrsvwx";
#else
static char optstr[] = "0123456789A:B:C:EFHLVce:f:hilnqsvwx";
#endif /* !WITHOUT_FTS */

static struct option long_options[] =
{
#ifndef NO_COLOR
    {"color",               required_argument, NULL, COLOR_OPT},
    {"colour",              required_argument, NULL, COLOR_OPT},
#endif /* !NO_COLOR */
    {"binary-files",        required_argument, NULL, BINARY_OPT},
    {"input",               required_argument, NULL, INPUT_OPT},
    {"reader",              required_argument, NULL, READER_OPT},
    {"after-context",       required_argument, NULL, 'A'},
    {"before-context",      required_argument, NULL, 'B'},
    {"context",             required_argument, NULL, 'C'},
    {"extended-regexp",     no_argument,       NULL, 'E'}, // POSIX
    {"fixed-string",        no_argument,       NULL, 'F'}, // POSIX
    {"with-filename",       no_argument,       NULL, 'H'},
    {"files-without-match", no_argument,       NULL, 'L'},
#ifndef WITHOUT_FTS
    {"recursive",           no_argument,       NULL, 'R'},
#endif /* !WITHOUT_FTS */
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
#ifndef WITHOUT_FTS
    {"recursive",           no_argument,       NULL, 'r'},
#endif /* !WITHOUT_FTS */
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
        "usage: %s [-0123456789EFHLRVchilnqrsvwx] [-A num] [-B num]\n"
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
            if (!add_pattern(error, l, ustr->ptr, ustr->len, pattern_type, flags)) {
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

static const UChar reset[] = {0x001b, 0x005b, 0x0030, 0x006d, U_NUL};
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

        if (snprintf(preffile, sizeof(preffile), "%s%c%s", home, DIRECTORY_SEPARATOR, ".ugrep") < (int) sizeof(preffile)) {
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
                                                *c->value = U_NUL;
# endif /* _MSC_VER */
                                                break;
                                            } else {
                                                for (a = attrs; a->name; a++) {
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
        u_fputc(U_LF, ustdout); // TODO: system dependant
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
        u_fputc(U_LF, ustdout); // TODO: system dependant
    }
}

#ifdef DEBUG
void fixed_circular_list_print(fixed_circular_list_t *l) /* NONNULL() */
{
    size_t i;

    require_else_return(NULL != l);

    u_printf("| i |   ADDR   | USED | PTR | HEAD | USTRING\n");
    u_printf("--------------------------------------------\n");
    for (i = 0; i < l->len; i++) {
        FETCH_DATA(l->elts[i].data, x, line_t);
        u_printf("| %d | %p | %4d | %3d | %4d | %S\n", i, &l->elts[i], l->elts[i].used, &l->elts[i] == l->ptr, &l->elts[i] == l->head, x->ustr->ptr);
    }
}
#endif /* DEBUG */

static int procfile(reader_t *reader, const char *filename)
{
    UString *ustr;
    error_t *error;
#ifndef NO_COLOR
    UBool _colorize;
#endif /* !NO_COLOR */
    size_t last_line_print;
    int arg_matches; // matches (for the current file) against command arguments (-v)
    int _after_context;

    error = NULL;
    arg_matches = 0;
    _after_context = 0;
    last_line_print = 0;
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
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            pattern_matches = 0;
            reader->lineno++;
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
                    pattern_matches++;
                    break; // no need to continue (line level)
                } else {
                    pattern_matches += ret;
                }
                //if (pattern_matches > 0 && (lFlag || (!vFlag && fd->binary && BIN_FILE_BIN == binbehave))) {
                if (pattern_matches && (lFlag || (reader->binary && BIN_FILE_BIN == binbehave))) {
                    debug("file skipping");
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
                if (_colorize && pattern_matches) {
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
# endif /* !_MSC_VER */
#endif /* !NO_COLOR */
                if (line->match) {
                    int i;
                    flist_element_t *el;

                    if ( (before_context || after_context) && last_line_print > before_context && (reader->lineno - before_context > last_line_print + 1) ) {
                        const UChar linesep[] = {SEP_NO_MATCH_UCHAR, SEP_NO_MATCH_UCHAR, U_NUL};

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
                            u_fputs(l->ustr->ptr, ustdout);
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
                        u_fputs(ustr->ptr, ustdout);
                        last_line_print = reader->lineno;
                        _after_context--;
                    }
                }
            }
        }
endfile:
        if (!_line_print) {
            if (cFlag) {
                if (file_print) {
                    print_file(reader->sourcename, arg_matches == 0, FALSE, TRUE, FALSE);
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
        reader_close(reader);
    } else {
        print_error(error);
    }

    return arg_matches;
}

#ifndef WITHOUT_FTS
static int procdir(reader_t *reader, char **dirname)
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
                matches += procfile(reader, p->fts_path);
                break;
        }
    }
    fts_close(fts);

    return matches;
}
#endif /* !WITHOUT_FTS */

/* ========== main ========== */

static void exit_cb(void)
{
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
    int lastc;
    UBool newarg;
    int prevoptind;
    reader_t reader;
#ifndef NO_COLOR
    int color;
#endif /* !NO_COLOR */
    int matches;
    UBool iFlag;
    UBool wFlag;
#ifndef WITHOUT_FTS
    UBool rFlag = FALSE;
#endif /* !WITHOUT_FTS */
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

    reader_init(&reader, "mmap");
    patterns = slist_new(pattern_destroy);
    exit_failure_value = UGREP_EXIT_FAILURE;

    //ustdio_init();

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
                    long val;
                    char *endptr;

                    val = strtol(optarg, &endptr, 10);
                    if (0 != errno || endptr == optarg || *endptr != '\0' || val </*=*/ 0) {
                        fprintf(stderr, "Context out of range\n");
                        return UGREP_EXIT_USAGE;
                    }
                    if ('A' != c) {
                        before_context = val;
                    }
                    if ('B' != c) {
                        after_context = val;
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
#ifndef WITHOUT_FTS
            case 'R':
                rFlag = TRUE;
                break;
#endif /* !WITHOUT_FTS */
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
#ifndef WITHOUT_FTS
            case 'r':
                rFlag = TRUE;
                break;
#endif /* !WITHOUT_FTS */
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
                if (!reader_set_imp_by_name(&reader, optarg)) {
                    fprintf(stderr, "Unknown reader\n");
                    return UGREP_EXIT_USAGE;
                }
                break;
            case INPUT_OPT:
                reader_set_default_encoding(&reader, optarg);
                break;
            default:
                usage();
                break;
        }
        lastc = c;
        newarg = optind != prevoptind;
        prevoptind = optind;
    }
    argc -= optind;
    argv += optind;

    reader_set_binary_behavior(&reader, binbehave);

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
            if (!add_patternC(&error, patterns, *argv++, pattern_type, flags)) {
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

    lines = fixed_circular_list_new(before_context + 1, line_ctor, line_dtor);
    intervals = intervals_new();

    if (0 == argc) {
        matches = procfile(&reader, "-");
#ifndef WITHOUT_FTS
    } else if (rFlag) {
        matches = procdir(&reader, argv);
#endif /* !WITHOUT_FTS */
    } else {
        for ( ; argc--; ++argv) {
            matches += procfile(&reader, *argv);
        }
    }

    return (matches > 0 ? UGREP_EXIT_MATCH : UGREP_EXIT_NO_MATCH);
}
