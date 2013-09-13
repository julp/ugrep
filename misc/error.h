#ifndef ERROR_H

# define ERROR_H

enum {
    INFO,
    WARN,
    FATAL
};

#ifdef DEBUG
const char *ubasename(const char *);

# define msg(type, format, ...) \
    report(type, "%s:%d:" format GRAY(" in %s()\n"), ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)

# define debug(format, ...) \
    msg(INFO, format, ## __VA_ARGS__)

# define UGREP_FILE_LINE_FUNC_D \
    const char *__ugrep_file, const unsigned int __ugrep_line, const char *__ugrep_func

# define UGREP_FILE_LINE_FUNC_DC \
    UGREP_FILE_LINE_FUNC_D,

# define UGREP_FILE_LINE_FUNC_C \
    __FILE__, __LINE__, __func__

# define UGREP_FILE_LINE_FUNC_CC \
    UGREP_FILE_LINE_FUNC_C,

# define UGREP_FILE_LINE_FUNC_RELAY_C \
    __ugrep_file, __ugrep_line, __ugrep_func

# define UGREP_FILE_LINE_FUNC_RELAY_CC \
    UGREP_FILE_LINE_FUNC_RELAY_C,

#else

# define msg(type, format, ...) \
    report(type, format "\n", ## __VA_ARGS__)

# define debug(format, ...)            /* NOP */
# define stdio_debug(format, ...)      /* NOP */
# define UGREP_FILE_LINE_FUNC_D        /* NOP */
# define UGREP_FILE_LINE_FUNC_DC       /* NOP */
# define UGREP_FILE_LINE_FUNC_C        /* NOP */
# define UGREP_FILE_LINE_FUNC_CC       /* NOP */
# define UGREP_FILE_LINE_FUNC_RELAY_C  /* NOP */
# define UGREP_FILE_LINE_FUNC_RELAY_CC /* NOP */
#endif /* DEBUG */

// <COMPAT>
// For compatibility, redirect icu_error_set to error_icu_set
// grep --exclude-dir=old -rn icu_error_set .
# define icu_error_set(error, type, status, function, ...) \
    error_icu_set(error, type, status, NULL, NULL, function, NULL)
// </COMPAT>

# define icu_msg(type, status, function) \
    msg(type, "ICU Error \"%s\" from " function "()", u_errorName(status))


typedef struct {
    int type;
    UChar *message;
} error_t;

#define error_set(/*error_t ** */ error, /*int*/ type, /*const char * */ format, ...) \
    _error_set(UGREP_FILE_LINE_FUNC_CC error, type, format, ## __VA_ARGS__)

#define error_icu_set(/*error_t ** */ error, /*int*/ type, /*UErrorCode*/ status, /*UParseError * */ pe, /*const UChar * */ pattern, /*const char * */ function, /*const char * */ format, ...) \
    _error_icu_set(UGREP_FILE_LINE_FUNC_CC error, type, status, pe, pattern, function, format, ## __VA_ARGS__)

void error_destroy(error_t *);
void error_propagate(error_t **, error_t *);

error_t *error_new(UGREP_FILE_LINE_FUNC_DC int, const char *, ...) WARN_UNUSED_RESULT;
error_t *error_vnew(UGREP_FILE_LINE_FUNC_DC int, const char *, va_list) WARN_UNUSED_RESULT;
void _error_set(UGREP_FILE_LINE_FUNC_DC error_t **, int, const char *, ...);

error_t *error_icu_new(UGREP_FILE_LINE_FUNC_DC int, UErrorCode, UParseError *, const UChar *, const char *, const char *, ...) WARN_UNUSED_RESULT;
error_t *error_icu_vnew(UGREP_FILE_LINE_FUNC_DC int, UErrorCode, UParseError *, const UChar *, const char *, const char *, va_list) WARN_UNUSED_RESULT;
void _error_icu_set(UGREP_FILE_LINE_FUNC_DC error_t **, int, UErrorCode, UParseError *, const UChar *, const char *, const char *, ...);

#ifdef _MSC_VER
error_t *error_win32_new(int, const char *, ...) WARN_UNUSED_RESULT;
error_t *error_win32_vnew(int, const char *, va_list) WARN_UNUSED_RESULT;
# ifdef DEBUG
#   define error_win32_set(error, type, format, ...) \
        _error_win32_set(error, type, "%s:%d:" format GRAY(" in %s()"), ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)
void _error_win32_set(error_t **, int, UErrorCode, UParseError *, const char *, ...);
# else
void error_win32_set(error_t **, int, const char *, ...);
# endif /* DEBUG */
#endif /* _MSC_VER */

#endif /* !ERROR_H */
