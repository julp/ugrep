#ifndef COMMON_H

# define COMMON_H

# include "config.h"

# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <stdarg.h>
# include <limits.h>

# include <unicode/utypes.h>
# include <unicode/uchar.h>
# include <unicode/ucnv.h>
# include <unicode/ustdio.h>
# include <unicode/ustring.h>
# include <unicode/unorm.h>


# ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
# else
#  define GCC_VERSION 0
# endif /* __GNUC__ */

# ifndef __has_attribute
#  define __has_attribute(x) 0
# endif /* !__has_attribute */

# if __GNUC__ || __has_attribute(unused)
#  define UNUSED(x) UNUSED_ ## x __attribute__((unused))
# else
#  define UNUSED
# endif /* UNUSED */

# if GCC_VERSION >= 3004 || __has_attribute(warn_unused_result)
#  define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
# else
#  define WARN_UNUSED_RESULT
# endif /* WARN_UNUSED_RESULT */

# if (GCC_VERSION >= 3003 || __has_attribute(nonnull)) && !defined(DEBUG)
#  define NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
# else
#  define NONNULL(...)
# endif /* NONNULL */

# if GCC_VERSION >= 2003 || __has_attribute(format)
#  define FORMAT(archetype, string_index, first_to_check) __attribute__((format(archetype, string_index, first_to_check)))
#  define PRINTF(string_index, first_to_check) FORMAT(__printf__, string_index, first_to_check)
# else
#  define FORMAT(archetype, string_index, first_to_check)
#  define PRINTF(string_index, first_to_check)
# endif /* FORMAT,PRINTF */

# ifdef _MSC_VER
#  define inline _inline
#  define __func__ __FUNCTION__
#  define MAXPATHLEN _MAX_PATH
#  define isatty _isatty
#  define fileno _fileno
#  define fdopen _fdopen
#  define open _open
#  define close _close
#  define isblank isspace
#  define snprintf sprintf_s
#  define strcasecmp _stricmp
#  define stat _stat
#  define fstat _fstat
#  define DIRECTORY_SEPARATOR '\\'
#  define PRIszu "Iu"
extern char __progname[];
# else
#  if !defined(MAXPATHLEN) && defined(PATH_MAX)
#   define MAXPATHLEN PATH_MAX
#  endif /* !MAXPATHLEN && PATH_MAX */
#  define DIRECTORY_SEPARATOR '/'
#  define PRIszu "zu"
extern char *__progname;
# endif /* _MSC_VER */

# define ensure(expr)                                                                                           \
    do {                                                                                                        \
        if (expr) {                                                                                             \
        } else {                                                                                                \
            fprintf(stderr, "[%s:%d]: assertion \"%s\" failed in %s()\n", __FILE__, __LINE__, #expr, __func__); \
            env_close();                                                                                        \
            exit(EXIT_FAILURE);                                                                                 \
        }                                                                                                       \
    } while (0);

# ifdef DEBUG
#  undef NDEBUG
#  define require_else_return(expr)                                                                                        \
    do {                                                                                                                   \
        if (expr) {                                                                                                        \
        } else {                                                                                                           \
            fprintf(stderr, "[%s:%d]: assertion \"%s\" failed in %s()\n", ubasename(__FILE__), __LINE__, #expr, __func__); \
            return;                                                                                                        \
        }                                                                                                                  \
    } while (0);

#  define require_else_return_val(expr, val)                                                                               \
    do {                                                                                                                   \
        if (expr) {                                                                                                        \
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
#  ifndef NDEBUG
#   define NDEBUG
#  endif /* !NDEBUG */
#  define require_else_return(expr)
#  define require_else_return_val(expr, val)
#  define require_else_return_null(expr)
#  define require_else_return_zero(expr)
#  define require_else_return_true(expr)
#  define require_else_return_false(expr)
# endif /* DEBUG */
# include <assert.h>

# define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
# define STR_LEN(str)      (ARRAY_SIZE(str) - 1)
# define STR_SIZE(str)     (ARRAY_SIZE(str))

# define U_BS 0x0008 /* Backspace */
# define U_HT 0x0009 /* Horizontal Tabulation - \t */
# define U_CR 0x000D /* Carriage Return - \r */
# define U_LF 0x000A /* Line Feed - \n */
# define U_VT 0x000B /* Vertical Tabulation */
# define U_FF 0x000C /* Form Feed */
# define U_0  0x0030 /* 0 */
# define U_9  0x0039 /* 9 */
# define U_A  0x0041 /* A */
# define U_F  0x0046 /* F */
# define U_U  0x0055 /* U */
# define U_a  0x0061 /* a */
# define U_f  0x0066 /* f */
# define U_u  0x0075 /* u */
# define U_NL 0x0085 /* Next Line */
# define U_LS 0x2028 /* Line Separator */
# define U_PS 0x2029 /* Paragraph Separator */

# ifdef _MSC_VER
static const UChar EOL[] = {U_CR, U_LF, 0};
# else
static const UChar EOL[] = {U_LF, 0};
# endif /* _MSC_VER */
static const size_t EOL_LEN = ARRAY_SIZE(EOL) - 1;

# if defined(DEBUG) && !defined(_MSC_VER)
#  define RED(str)    "\33[1;31m" str "\33[0m"
#  define GREEN(str)  "\33[1;32m" str "\33[0m"
#  define YELLOW(str) "\33[1;33m" str "\33[0m"
#  define GRAY(str)   "\33[1;30m" str "\33[0m"
# else
#  define RED(str)    str
#  define GREEN(str)  str
#  define YELLOW(str) str
#  define GRAY(str)   str
# endif /* DEBUG && !_MSC_VER */

typedef void *(*func_ctor_t)(void);        /* Constructor callback */
typedef void (*func_dtor_t)(void *);       /* Destructor callback */
typedef void *(*func_dup_t)(const void *); /* Dup, clone, copy callback */
typedef func_dup_t func_cpy_t;             /* Alias */

# ifndef MAX
#  define MAX(a, b) ((a) > (b) ? (a) : (b))
# endif /* !MAX */

# ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
# endif /* !MIN */

# define HAS_FLAG(value, flag) \
    (0 != ((value) & (flag)))

# define SET_FLAG(value, flag) \
    ((value) |= (flag))

# define UNSET_FLAG(value, flag) \
    ((value) &= ~(flag))

# define FETCH_DATA(from, to, type) \
    type *to = (type *) (from)

# ifdef _MSC_VER
#  define OLD_INTERVAL 1
#  define OLD_RING     1
# endif /* _MSC_VER */

# define COPYRIGHT "\nCopyright (C) 2010-2013, julp\n"

# include "alloc.h"

# if SIZEOF_LONG == SIZEOF_VOIDP
typedef unsigned long dup_t;
# elif SIZEOF_LONG_LONG == SIZEOF_VOIDP
typedef unsigned long long dup_t;
# else
#  error (sizeof(void*) == sizeof(long) || sizeof(void*) == sizeof(long long)) required to be compiled
# endif

static const dup_t NODUP = 0;

#define SIZE_TO_DUP_T(size) \
    (((size) == 0) ? NODUP : (dup_t)(((long)(size)) << 1 | 1))

static inline void *clone(dup_t duper, void *value) {
    if (NODUP == duper) {
        return value;
    } else if (duper & 1) {
        void *copy;
        unsigned long size;

        size = (duper >> 1) & LONG_MAX;
        copy = _mem_alloc(size);
        memcpy(copy, value, size);
        return copy;
    } else {
        return ((func_dup_t) duper)(value);
    }
}

# include "error.h"
# include "ustring.h"
# ifdef DEBUG
typedef void (*toUString)(UString *, const void *, const void *);
# endif /* DEBUG */
# include "io/reader.h"
# include "env.h"
# include "util.h"

#endif /* !COMMON_H */
