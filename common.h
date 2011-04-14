#ifndef COMMON_H

# define COMMON_H

# include "config.h"

# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <stdarg.h>

# include <unicode/utypes.h>
# include <unicode/ucnv.h>
# include <unicode/ustdio.h>
# include <unicode/ustring.h>


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

# if GCC_VERSION >= 2003
#  define FORMAT(archetype, string_index, first_to_check) __attribute__((format(archetype, string_index, first_to_check)))
#  define PRINTF(string_index, first_to_check) FORMAT(__printf__, string_index, first_to_check)
# else
#  define FORMAT(archetype, string_index, first_to_check)
#  define PRINTF(string_index, first_to_check)
# endif /* FORMAT,PRINTF */

# ifdef _MSC_VER
#  define CCALL __cdecl
#  pragma section(".CRT$XCU",read)
#  define INITIALIZER_DECL(f) \
    void __cdecl f(void); \
    __declspec(allocate(".CRT$XCU")) void (__cdecl*f##_)(void) = f
# elif defined(__GNUC__)
#  define CCALL
#  define INITIALIZER_DECL(f) \
    void f(void) __attribute__((constructor))
# endif /* INITIALIZER_DECL */

# define INITIALIZER_P(f) \
    void CCALL f(void)


# ifdef _MSC_VER
#  define inline _inline
#  define __func__ __FUNCTION__
#  define MAXPATHLEN _MAX_PATH
#  define isatty _isatty
#  define fileno _fileno
#  define fdopen _fdopen
#  define isblank isspace
#  define snprintf sprintf_s
#  define DIRECTORY_SEPARATOR '\\'
extern char __progname[];
# else
#  define DIRECTORY_SEPARATOR '/'
extern char *__progname;
# endif /* _MSC_VER */

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

# define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

# define U_BS  0x0008 /* backspace */
# define U_CR  0x000D /* \r */
# define U_LF  0x000A /* \n */
# define U_NUL 0x0000 /* \0 */

# if defined(DEBUG) && !defined(_MSC_VER)
#  define RED(str)    "\33[1;31m" str "\33[0m"
#  define GREEN(str)  "\33[1;32m" str "\33[0m"
#  define YELLOW(str) "\33[1;33m" str "\33[0m"
# else
#  define RED(str)    str
#  define GREEN(str)  str
#  define YELLOW(str) str
# endif /* DEBUG && !_MSC_VER */

typedef void *(*func_ctor_t)(void); /* Constructor callback */
typedef void (*func_dtor_t)(void *); /* Destructor callback */

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif /* !MAX */

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* !MIN */

# define FETCH_DATA(from, to, type) \
    type *to = (type *) (from)

# include "alloc.h"
# include "error.h"
# include "ustring.h"
# include "reader.h"
# include "fd.h"
# include "util.h"

#endif /* !COMMON_H */
