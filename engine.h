#ifndef UGREP_H

# define UGREP_H

# include "common.h"
# include "struct/slist.h"
# include "struct/intervals.h"
# include "struct/darray.h"

# define OPT_CASE_INSENSITIVE 0x00010000
# define OPT_WORD_BOUND       0x00020000
# define OPT_WHOLE_LINE_MATCH 0x00040000
# define OPT_NON_GRAPHEME     0x00080000
# define OPT_MASK             0xFFFF0000

# define IS_CASE_INSENSITIVE(flags) ((flags & OPT_CASE_INSENSITIVE))
# define IS_WHOLE_LINE(flags)       ((flags & OPT_WHOLE_LINE_MATCH))
# define IS_WORD_BOUNDED(flags)     ((flags & OPT_WORD_BOUND))

# define WITH_GRAPHEME()            (UNIT_CODEPOINT != env_get_unit())

typedef enum {
    ENGINE_FAILURE     = -1,
    ENGINE_NO_MATCH    =  0,
    ENGINE_MATCH_FOUND =  1,
    ENGINE_WHOLE_LINE_MATCH
} engine_return_t;

typedef struct {
    UChar *ptr;
    size_t len;
} match_t;

static inline void add_match(DArray *array, const UString *subject, int32_t l, int32_t u)
{
    match_t m;

    m.ptr = subject->ptr + l;
    m.len = u - l;
debug("add_match [%d;%d[ for >%S< (>%.*S< ; %d)", l, u, subject->ptr, m.len, m.ptr, m.len);
    darray_push(array, m);
}

typedef struct {
    void *(*compile)(error_t **, UString *, uint32_t); /* /!\ The UString will be owned by the engine: it can be freed at any time depending on the internal behavior of the engine /!\ */
    engine_return_t (*match)(error_t **, void *, const UString *);
    engine_return_t (*match_all)(error_t **, void *, const UString *, interval_list_t *);
    engine_return_t (*whole_line_match)(error_t **, void *, const UString *);
    UBool (*split)(error_t **, void *, const UString *, DArray *, interval_list_t *);
    void (*destroy)(void *);
} engine_t;

typedef struct {
    void *pattern;
    engine_t *engine;
} pattern_data_t;

#endif /* !UGREP_H */
