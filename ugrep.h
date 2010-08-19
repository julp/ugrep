#ifndef UGREP_H

# define UGREP_H

# include <stdlib.h>
# include <stdio.h>

# include <unicode/utypes.h>
# include <unicode/ucnv.h>

# ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
# else
#  define GCC_VERSION 0
# endif /* __GNUC__ */

# if GCC_VERSION >= 2000
#  define EXPECTED(condition) __builtin_expect(!!(condition), 1)
#  define UNEXPECTED(condition) __builtin_expect(!!(condition), 0)
# else
#  define EXPECTED(condition) (condition)
#  define UNEXPECTED(condition) (condition)
# endif /* (UN)EXPECTED */

# define ensure(expr)                                                                                            \
    do {                                                                                                         \
        if (EXPECTED(expr)) {                                                                                    \
        } else {                                                                                                 \
            fprintf(stderr, "[%s: %d]: assertion \"%s\" failed in %s()\n", __FILE__, __LINE__, #expr, __func__); \
            exit(EXIT_FAILURE);                                                                                  \
        }                                                                                                        \
    } while (0);

#ifdef DEBUG
# include <libgen.h> /* for (unsafe) basename */
# define msg(format, ...) \
    fprintf(stderr, "[ERROR] %s:%d:" format " in %s()\n", basename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)
    //fprintf(stderr, "[ERROR] " __FILE__ ":%d:" format " in %s()\n", __LINE__, ## __VA_ARGS__, __func__)

#define debug(format, ...) \
    fprintf(stderr, "[DEBUG] %s:%d:" format " in %s()\n", basename(__FILE__), __LINE__, ## __VA_ARGS__, __func__)
    //fprintf(stderr, "[DEBUG] " __FILE__ ":%d:" format " in %s()\n", __LINE__, ## __VA_ARGS__, __func__)
#else
# define msg(format, ...) \
    fprintf(stderr, "[ERROR] " format "\n", ## __VA_ARGS__)

# define debug(format, ...) /* NOP */
#endif /* DEBUG */

# define icu(status, function) \
    msg("ICU Error \"%s\" from " function "()", u_errorName(status))


# define U_BS  0x0008 /* backspace */
# define U_CR  0x000D /* \r */
# define U_LF  0x000A /* \n */
# define U_EOB 0x0000 /* \0 */

# include "config.h"
# include "alloc.h"
# include "ustring.h"

UBool is_binary_uchar(UChar32);

# define FETCH_READER_DATA(from, to, type) \
    type *to = (type *) from

//# define WITH_IS_BINARY 1

typedef struct {
    void *(*open)(const char *);
    void (*close)(void *);
    UBool (*readline)(void *, UString *); // add boolean to copy or not (\r)\n ?
    size_t (*readbytes)(void *, char *, size_t);
# ifdef WITH_IS_BINARY
    int (*is_binary)(void *, size_t); // revert to is_binary instead of readuchars ?
# else
    size_t (*readuchars)(void *, UChar32 *, size_t);
# endif /* WITH_IS_BINARY */
    void (*set_signature_length)(void *, size_t);
    void (*set_encoding)(void *, const char *);
    void (*rewind)(void *);
    // add UBool (*eof)(void *); ?
} reader_t;

#endif /* !UGREP_H */
