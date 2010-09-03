#ifndef UGREP_H

# define UGREP_H

# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <stdarg.h>

# include <unicode/utypes.h>
# include <unicode/ucnv.h>
# include <unicode/ustdio.h>
# include <unicode/ustring.h>
# include <unicode/ucsdet.h>
# include <unicode/uregex.h>

# ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
# else
#  define GCC_VERSION 0
# endif /* __GNUC__ */

# ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((unused))
# else
#  define UNUSED
# endif /* UNUSED */

# if GCC_VERSION >= 2000
#  define EXPECTED(condition) __builtin_expect(!!(condition), 1)
#  define UNEXPECTED(condition) __builtin_expect(!!(condition), 0)
# else
#  define EXPECTED(condition) (condition)
#  define UNEXPECTED(condition) (condition)
# endif /* (UN)EXPECTED */

# define ensure(expr)                                                                                           \
    do {                                                                                                        \
        if (EXPECTED(expr)) {                                                                                   \
        } else {                                                                                                \
            fprintf(stderr, "[%s:%d]: assertion \"%s\" failed in %s()\n", __FILE__, __LINE__, #expr, __func__); \
            exit(EXIT_FAILURE);                                                                                 \
        }                                                                                                       \
    } while (0);

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

# define error(error, type, format, ...) \
    error_set(error, type, "%s:%d:" format " in %s()\n", ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)

# define u_printf(...)                                \
    do {                                              \
        UFILE *ustdout = u_finit(stdout, NULL, NULL); \
        u_fprintf(ustdout, ## __VA_ARGS__);           \
    } while (0);
#else
# define msg(type, format, ...) \
    report(type, format "\n", ## __VA_ARGS__)

# define error(error, type, format, ...) \
    error_set(error, type, format "\n", ## __VA_ARGS__)

# define debug(format, ...) /* NOP */
#endif /* DEBUG */

/* TODO: drop icu(), replace it by icu_error() */
# define icu(status, function) \
    msg(FATAL, "ICU Error \"%s\" from " function "()", u_errorName(status))

# define icu_error(error, type, status, function) \
    error(error, type, "ICU Error \"%s\" from " function "()", u_errorName(status))

# define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

# define U_BS  0x0008 /* backspace */
# define U_CR  0x000D /* \r */
# define U_LF  0x000A /* \n */
# define U_NUL 0x0000 /* \0 */

# include "config.h"
# include "alloc.h"
# include "slist.h"
# include "ustring.h"
# include "intervals.h"

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif /* !MAX */

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* !MIN */

# define FETCH_DATA(from, to, type) \
    type *to = (type *) (from)

/* <error.c> */
typedef struct {
    int type;
    UChar *message;
} error_t;

void error_destroy(error_t *);
error_t *error_new(int, const char *, ...);
void error_propagate(error_t **, error_t *);
void error_set(error_t **, int, const char *, ...);
error_t *error_vnew(int, const char *, va_list);
/* </error.c> */

typedef struct {
    const char *name;
    void *(*open)(const char *, int); // can throw error
    void (*close)(void *);
    UBool (*eof)(void *);
    UBool (*seekable)(void *);
    UBool (*readline)(void *, UString *); // can throw error
    size_t (*readbytes)(void *, char *, size_t);
    size_t (*readuchars)(void *, UChar32 *, size_t); // can throw error
    void (*set_signature_length)(void *, size_t);
    void (*set_encoding)(void *, const char *); // can throw error
    void (*rewind)(void *);
} reader_t;

typedef enum {
    ENGINE_FAILURE     = -1,
    ENGINE_NO_MATCH    =  0,
    ENGINE_MATCH_FOUND =  1,
    ENGINE_WHOLE_LINE_MATCH
} engine_return_t;

typedef struct {
    void *(*compile)(error_t **, const UChar *, int32_t, UBool, UBool);
    void *(*compileC)(error_t **, const char *, UBool, UBool);
    void (*pre_exec)(void *, UString *);
    engine_return_t (*match)(error_t **, void *, const UString *);
    engine_return_t (*match_all)(error_t **, void *, const UString *, slist_t *);
    engine_return_t (*whole_line_match)(error_t **, void *, const UString *);
    void (*destroy)(void *);
} engine_t;

typedef struct {
    void *pattern;
    engine_t *engine;
} pattern_data_t;

/* <ugrep.c> */
void report(int type, const char *format, ...);
/* </ugrep.c> */

#endif /* !UGREP_H */
