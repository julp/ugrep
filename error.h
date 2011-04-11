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
    report(type, "%s:%d:" format " in %s()\n", ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)

# define debug(format, ...) \
    msg(INFO, format, ## __VA_ARGS__)

# define u_printf(...)                                \
    do {                                              \
        UFILE *ustdout = u_finit(stdout, NULL, NULL); \
        u_fprintf(ustdout, ## __VA_ARGS__);           \
    } while (0);
#else
# define msg(type, format, ...) \
    report(type, format "\n", ## __VA_ARGS__)

# define debug(format, ...) /* NOP */
#endif /* DEBUG */

# define icu_error_set(error, type, status, function) \
    error_set(error, type, "ICU Error \"%s\" from " function "()", u_errorName(status))


typedef struct {
    int type;
    UChar *message;
} error_t;


void error_destroy(error_t *);
error_t *error_new(int, const char *, ...) WARN_UNUSED_RESULT;
void error_propagate(error_t **, error_t *);
# ifdef DEBUG
#  define error_set(error, type, format, ...) \
        _error_set(error, type, "%s:%d:" format " in %s()", ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)
void _error_set(error_t **, int, const char *, ...);
#  ifdef _MSC_VER
#   define error_win32_set(error, type, format, ...) \
        _error_win32_set(error, type, "%s:%d:" format " in %s()", ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)
void _error_win32_set(error_t **, int, const char *, ...);
#  endif /* _MSC_VER */
# else
void error_set(error_t **, int, const char *, ...);
#  ifdef _MSC_VER
void error_win32_set(error_t **, int, const char *, ...);
#  endif /* _MSC_VER */
# endif /* DEBUG */
error_t *error_vnew(int, const char *, va_list) WARN_UNUSED_RESULT;

/* <ugrep.c> */
//void report(int type, const char *format, ...);
/* </ugrep.c> */

#endif /* !ERROR_H */
