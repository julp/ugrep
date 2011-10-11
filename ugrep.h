#ifndef UGREP_H

# define UGREP_H

//# define OLD_INTERVAL 1

# include "common.h"
# include "struct/slist.h"
# include "struct/intervals.h"
# include "struct/fixed_circular_list.h"

# define OPT_CASE_INSENSITIVE 0x00010000
# define OPT_WORD_BOUND       0x00020000
# define OPT_WHOLE_LINE_MATCH 0x00040000
# define OPT_MASK             0xFFFF0000

# define IS_CASE_INSENSITIVE(flags) ((flags & OPT_CASE_INSENSITIVE))
# define IS_WHOLE_LINE(flags)       ((flags & OPT_WHOLE_LINE_MATCH))
# define IS_WORD_BOUNDED(flags)     ((flags & OPT_WORD_BOUND))

typedef enum {
    ENGINE_FAILURE     = -1,
    ENGINE_NO_MATCH    =  0,
    ENGINE_MATCH_FOUND =  1,
    ENGINE_WHOLE_LINE_MATCH
} engine_return_t;

typedef struct {
    void *(*compile)(error_t **, const UChar *, int32_t, uint32_t);
    void *(*compileC)(error_t **, const char *, uint32_t);
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

#endif /* !UGREP_H */
