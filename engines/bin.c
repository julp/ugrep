#include "engine.h"

#include <unicode/ubrk.h>

// defined in engines/fixed.c
UBool binary_fwd_n(UBreakIterator *, const UString *, const UString *, DArray *, size_t, int32_t *);

typedef struct {
    uint32_t flags;
    UString *tmp; /* a temporary buffer for full case folding */
    UString *pattern;
    UBreakIterator *ubrk;
} bin_pattern_t;

static void bin_pattern_destroy(bin_pattern_t *p)
{
    if (NULL != p->tmp) {
        ustring_destroy(p->tmp);
    }
    if (NULL != p->ubrk) {
        ubrk_close(p->ubrk);
    }
    ustring_destroy(p->pattern);
    free(p);
}

# define CASE_FOLD_SUBJECT_IF_NEEDED(/*error_t ***/ error, /*bin_pattern_t **/p, /*UString **/ subject) \
    do {                                                                                                \
        if (NULL != p->tmp) {                                                                           \
            if (!ustring_fullcase(p->tmp, subject->ptr, subject->len, UCASE_FOLD, error)) {             \
                return ENGINE_FAILURE;                                                                  \
            }                                                                                           \
            subject = p->tmp;                                                                           \
        }                                                                                               \
    } while (0);

/**
 * With full case folding, when we work with string offsets, results may be wrong
 * Don't forget that a single code point can give up to three code points
 * Eg: if we search "Straße" in "123Straße456" match will be on "Straße4"
 * Because Straße is expanded to strasse, so the length has changed (ß < ss => + 1)
 **/
# define CASE_FOLD_FORBIDDEN(/*error_t ***/ error, /*bin_pattern_t **/p, return_value_if_forbidden) \
    do {                                                                                            \
        if (NULL != p->tmp) {                                                                       \
            error_set(error, FATAL, "Due to full case folding, results (offsets) may be wrong");    \
            return return_value_if_forbidden;                                                       \
        }                                                                                           \
    } while (0);

static void *engine_bin_compile(error_t **error, UString *ustr, uint32_t flags)
{
    UErrorCode status;
    bin_pattern_t *p;

    p = mem_new(*p);
    p->pattern = ustr;
    p->flags = flags;
    p->ubrk = NULL;
    p->tmp = NULL;
    status = U_ZERO_ERROR;
    if (ustring_empty(ustr)) {
        if (IS_WORD_BOUNDED(flags)) {
            p->ubrk = ubrk_open(UBRK_WORD, NULL, NULL, 0, &status);
        }
    } else {
        /**
         * Whole line matches are simplified:
         * 1) we don't need to check graphemes boundaries,
         * 2) case insensitivity is done directly by the whole_line_match callback
         **/
        if (!IS_WHOLE_LINE(flags)) {
            if (IS_WORD_BOUNDED(flags)) {
                p->ubrk = ubrk_open(UBRK_WORD, NULL, NULL, 0, &status);
            } else {
                p->ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
            }
            if (U_FAILURE(status)) {
                bin_pattern_destroy(p);
                icu_error_set(error, FATAL, status, "ubrk_open");
                return NULL;
            }
            if (IS_CASE_INSENSITIVE(flags)) {
                p->tmp = ustring_new();
                p->pattern = ustring_sized_new(ustr->len);
                if (!ustring_fullcase(p->pattern, ustr->ptr, ustr->len, UCASE_FOLD, error)) {
                    bin_pattern_destroy(p);
                    return NULL;
                }
                ustring_destroy(ustr); /* no more needed, throw (free) it now */
            }
        }
    }

    return p;
}

static engine_return_t engine_bin_match(error_t **error, void *data, const UString *subject)
{
    int32_t ret;
    UErrorCode status;
    FETCH_DATA(data, p, bin_pattern_t);

    status = U_ZERO_ERROR;
    CASE_FOLD_SUBJECT_IF_NEEDED(error, p, subject);
    if (ustring_empty(p->pattern)) {
        if (IS_WORD_BOUNDED(p->flags)) {
            if (ustring_empty(subject)) {
                return ENGINE_MATCH_FOUND;
            } else {
                int32_t l, u, lastState, state;

                ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
                if (U_FAILURE(status)) {
                    icu_error_set(error, FATAL, status, "ubrk_setText");
                    return ENGINE_FAILURE;
                }
                if (UBRK_DONE != (l = ubrk_first(p->ubrk))) {
                    lastState = ubrk_getRuleStatus(p->ubrk);
                    while (UBRK_DONE != (u = ubrk_next(p->ubrk))) {
                        state = ubrk_getRuleStatus(p->ubrk);
                        if (UBRK_WORD_NONE == lastState && lastState == state) {
                            return ENGINE_MATCH_FOUND;
                        }
                        lastState = state;
                        l = u;
                    }
                }
                return ENGINE_NO_MATCH;
            }
        } else {
            return ENGINE_MATCH_FOUND;
        }
    } else {
        UChar *m;
        int32_t pos;

        pos = 0;
        ret = ENGINE_NO_MATCH;
        ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_setText");
            return ENGINE_FAILURE;
        }
        while (NULL != (m = u_strFindFirst(subject->ptr + pos, subject->len - pos, p->pattern->ptr, p->pattern->len))) {
            pos = m - subject->ptr;
            if (ubrk_isBoundary(p->ubrk, pos) && ubrk_isBoundary(p->ubrk, pos + p->pattern->len)) {
                ret = ENGINE_MATCH_FOUND;
            }
            pos += p->pattern->len;
        }
        ubrk_unbindText(p->ubrk);

        return ret;
    }
}

static engine_return_t engine_bin_match_all(error_t **error, void *data, const UString *subject, interval_list_t *intervals)
{
    int32_t matches;
    UErrorCode status;
    FETCH_DATA(data, p, bin_pattern_t);

    matches = 0;
    status = U_ZERO_ERROR;
    CASE_FOLD_FORBIDDEN(error, p, ENGINE_FAILURE);
    if (ustring_empty(p->pattern)) {
        if (IS_WORD_BOUNDED(p->flags)) {
            if (ustring_empty(subject)) {
                return ENGINE_MATCH_FOUND;
            } else {
                int32_t l, u, lastState, state;

                ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
                if (U_FAILURE(status)) {
                    icu_error_set(error, FATAL, status, "ubrk_setText");
                    return ENGINE_FAILURE;
                }
                if (UBRK_DONE != (l = ubrk_first(p->ubrk))) {
                    lastState = ubrk_getRuleStatus(p->ubrk);
                    while (UBRK_DONE != (u = ubrk_next(p->ubrk))) {
                        state = ubrk_getRuleStatus(p->ubrk);
                        if (UBRK_WORD_NONE == lastState && lastState == state) {
                            return ENGINE_MATCH_FOUND;
                        }
                        lastState = state;
                        l = u;
                    }
                }
                return ENGINE_NO_MATCH;
            }
        } else {
            return ENGINE_MATCH_FOUND;
        }
    } else {
        UChar *m;
        int32_t pos;

        pos = 0;
        ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_setText");
            return ENGINE_FAILURE;
        }
        while (NULL != (m = u_strFindFirst(subject->ptr + pos, subject->len - pos, p->pattern->ptr, p->pattern->len))) {
            pos = m - subject->ptr;
            if (ubrk_isBoundary(p->ubrk, pos) && ubrk_isBoundary(p->ubrk, pos + p->pattern->len)) {
                matches++;
                if (interval_list_add(intervals, subject->len, pos, pos + p->pattern->len)) {
                    return ENGINE_WHOLE_LINE_MATCH;
                }
            }
            pos += p->pattern->len;
        }
        ubrk_unbindText(p->ubrk);

        return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
    }
}

static engine_return_t engine_bin_whole_line_match(error_t **UNUSED(error), void *data, const UString *subject)
{
    FETCH_DATA(data, p, bin_pattern_t);

    /* If search is case insensitive, we don't do case folding here, u_strcasecmp suffice (it does full case folding internally) */
    if (ustring_empty(p->pattern)) {
        return ustring_empty(subject) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH;
    } else {
        if (IS_CASE_INSENSITIVE(p->flags)) {
            return (0 == u_strcasecmp(p->pattern->ptr, subject->ptr, 0) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
        } else {
            return (0 == u_strcmp(p->pattern->ptr, subject->ptr) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
        }
    }
}

/**
 * Don't modify this function, it reproduces a part of fixed engine (engine_fixed_split)
 **/
static UBool engine_bin_split(error_t **error, void *data, const UString *subject, DArray *array, interval_list_t *intervals)
{
    UErrorCode status;
    int32_t l, lastU;
    dlist_element_t *el;
    FETCH_DATA(data, p, bin_pattern_t);

    lastU = l = 0;
    status = U_ZERO_ERROR;

    CASE_FOLD_FORBIDDEN(error, p, FALSE); /* the only engine specific thing */

    ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ubrk_setText");
        return FALSE;
    }
    for (el = intervals->head; NULL != el; el = el->next) {
        FETCH_DATA(el->data, i, interval_t);

        if (i->lower_limit > 0) {
            if (!binary_fwd_n(p->ubrk, p->pattern, subject, NULL, i->lower_limit - lastU, &l)) {
                break;
            }
        }
        if (!binary_fwd_n(p->ubrk, p->pattern, subject, array, i->upper_limit - i->lower_limit, &l)) {
            break;
        }
        lastU = i->upper_limit;
    }
    ubrk_unbindText(p->ubrk);

    return TRUE;
}

static void engine_bin_destroy(void *data)
{
    FETCH_DATA(data, p, bin_pattern_t);

    bin_pattern_destroy(p);
}

engine_t bin_engine = {
    engine_bin_compile,
    engine_bin_match,
    engine_bin_match_all,
    engine_bin_whole_line_match,
    engine_bin_split,
    engine_bin_destroy
};
