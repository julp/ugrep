#ifndef UGREP_H

# define UGREP_H

//# define OLD_INTERVAL 1

# include "config.h"

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

# include <unicode/uloc.h>
# include <unicode/ucol.h>
# include <unicode/ubrk.h>
# include <unicode/usearch.h>


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

# if GCC_VERSION >= 3004
#  define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
# else
#  define WARN_UNUSED_RESULT
# endif /* WARN_UNUSED_RESULT */

# if GCC_VERSION >= 3003 && !defined(DEBUG)
#  define NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define NONNULL(...)
# endif /* NONNULL */

# define ensure(expr)                                                                                           \
    do {                                                                                                        \
        if (EXPECTED(expr)) {                                                                                   \
        } else {                                                                                                \
            fprintf(stderr, "[%s:%d]: assertion \"%s\" failed in %s()\n", __FILE__, __LINE__, #expr, __func__); \
            exit(EXIT_FAILURE);                                                                                 \
        }                                                                                                       \
    } while (0);

# ifdef DEBUG
#  define require_else_return(expr)                                                                                        \
    do {                                                                                                                   \
        if (EXPECTED(expr)) {                                                                                              \
        } else {                                                                                                           \
            fprintf(stderr, "[%s:%d]: assertion \"%s\" failed in %s()\n", ubasename(__FILE__), __LINE__, #expr, __func__); \
            return;                                                                                                        \
        }                                                                                                                  \
    } while (0);

#  define require_else_return_val(expr, val)                                                                               \
    do {                                                                                                                   \
        if (EXPECTED(expr)) {                                                                                              \
        } else {                                                                                                           \
            fprintf(stderr, "[%s:%d]: assertion \"%s\" failed in %s()\n", ubasename(__FILE__), __LINE__, #expr, __func__); \
            return (val);                                                                                                  \
        }                                                                                                                  \
    } while (0);

#  define require_else_return_null(expr)  require_else_return_val(expr, NULL)
#  define require_else_return_zero(expr)  require_else_return_val(expr, 0)
#  define require_else_return_true(expr)  require_else_return_val(expr, TRUE)
#  define require_else_return_false(expr) require_else_return_val(expr, FALSE)
# else
#  define require_else_return(expr)
#  define require_else_return_val(expr, val)
#  define require_else_return_null(expr)
#  define require_else_return_zero(expr)
#  define require_else_return_true(expr)
#  define require_else_return_false(expr)
# endif /* DEBUG */

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

# define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

# define U_BS  0x0008 /* backspace */
# define U_CR  0x000D /* \r */
# define U_LF  0x000A /* \n */
# define U_NUL 0x0000 /* \0 */

typedef void *(*func_ctor_t)(void); /* Constructor callback */
typedef void (*func_dtor_t)(void *); /* Destructor callback */

# include "alloc.h"
# include "slist.h"
# include "fixed_circular_list.h"
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
error_t *error_new(int, const char *, ...) WARN_UNUSED_RESULT;
void error_propagate(error_t **, error_t *);
#ifdef DEBUG
# define error_set(error, type, format, ...) \
    _error_set(error, type, "%s:%d:" format " in %s()", ubasename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)
void _error_set(error_t **, int, const char *, ...);
#else
void error_set(error_t **, int, const char *, ...);
#endif /* DEBUG */
error_t *error_vnew(int, const char *, va_list) WARN_UNUSED_RESULT;
/* </error.c> */

# include "ustring.h"

typedef struct {
    const char *name;
    void *(*open)(error_t **, const char *, int);
    void (*close)(void *);
    UBool (*eof)(void *);
    UBool (*seekable)(void *);
    UBool (*readline)(error_t **error, void *, UString *);
    size_t (*readbytes)(void *, char *, size_t);
    int32_t (*readuchars)(error_t **error, void *, UChar32 *, size_t);
    void (*set_signature_length)(void *, size_t);
    UBool (*set_encoding)(error_t **, void *, const char *);
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
    engine_return_t (*match)(error_t **, void *, const UString *);
#ifdef OLD_INTERVAL
    engine_return_t (*match_all)(error_t **, void *, const UString *, slist_t *);
#else
    engine_return_t (*match_all)(error_t **, void *, const UString *, slist_pool_t *);
#endif /* OLD_INTERVAL */
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
