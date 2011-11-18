#include "engine.h"

#include <unicode/uloc.h>
#include <unicode/ucol.h>
#include <unicode/ubrk.h>
#include <unicode/usearch.h>

static UChar _USEARCH_FAKE_USTR[] = { 0, 0 };
#define USEARCH_FAKE_USTR _USEARCH_FAKE_USTR, 1 // empty stings refused by usearch

typedef struct {
    uint32_t flags;
    UString *pattern;
    UBreakIterator *ubrk;
    UStringSearch *usearch;
} fixed_pattern_t;

static void fixed_pattern_destroy(fixed_pattern_t *p)
{
    if (NULL != p->usearch) {
        usearch_close(p->usearch);
    }
    if (NULL != p->ubrk) {
        ubrk_close(p->ubrk);
    }
    ustring_destroy(p->pattern);
    free(p);
}

static void *engine_fixed_compile(error_t **error, UString *ustr, uint32_t flags)
{
    UErrorCode status;
    fixed_pattern_t *p;

    p = mem_new(*p);
    p->pattern = ustr; // not needed with usearch ?
    p->flags = flags;
    p->ubrk = NULL;
    p->usearch = NULL;
    status = U_ZERO_ERROR;
    if (ustring_empty(ustr)) {
        if (IS_WORD_BOUNDED(flags)) {
            p->ubrk = ubrk_open(UBRK_WORD, NULL, NULL, 0, &status);
        }
    } else {
        if (!IS_WHOLE_LINE(flags)) {
            if (IS_WORD_BOUNDED(flags)) {
                p->ubrk = ubrk_open(UBRK_WORD, NULL, NULL, 0, &status);
            } else {
                p->ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
            }
            if (U_FAILURE(status)) {
                fixed_pattern_destroy(p);
                icu_error_set(error, FATAL, status, "ubrk_open");
                return NULL;
            }
        }
        if (IS_WORD_BOUNDED(flags) || (IS_CASE_INSENSITIVE(flags) && !IS_WHOLE_LINE(flags))) {
            p->usearch = usearch_open(ustr->ptr, ustr->len, USEARCH_FAKE_USTR, uloc_getDefault(), p->ubrk, &status);
            if (U_FAILURE(status)) {
                if (NULL != p->ubrk) {
                    ubrk_close(p->ubrk);
                }
                fixed_pattern_destroy(p);
                icu_error_set(error, FATAL, status, "usearch_open");
                return NULL;
            }
            if (IS_CASE_INSENSITIVE(flags)) {
                UCollator *ucol;

                ucol = usearch_getCollator(p->usearch);
                ucol_setStrength(ucol, (flags & ~OPT_MASK) > 1 ? UCOL_SECONDARY : UCOL_PRIMARY);
            }
        }
    }

    return p;
}

static engine_return_t engine_fixed_match(error_t **error, void *data, const UString *subject)
{
    int32_t ret;
    UErrorCode status;
    FETCH_DATA(data, p, fixed_pattern_t);

    status = U_ZERO_ERROR;
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
    } else if (NULL != p->usearch) {
        if (subject->len > 0) {
            usearch_setText(p->usearch, subject->ptr, subject->len, &status);
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "usearch_setText");
                return ENGINE_FAILURE;
            }
            ret = usearch_first(p->usearch, &status);
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "usearch_first");
                return ENGINE_FAILURE;
            }
            usearch_unbindText(p->usearch);

            return (ret != USEARCH_DONE ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
        } else {
            return ENGINE_NO_MATCH;
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

static engine_return_t engine_fixed_match_all(error_t **error, void *data, const UString *subject, interval_list_t *intervals)
{
    int32_t matches;
    UErrorCode status;
    FETCH_DATA(data, p, fixed_pattern_t);

    matches = 0;
    status = U_ZERO_ERROR;
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
    } else if (NULL != p->usearch) {
        int32_t l, u;

        if (subject->len > 0) {
            usearch_setText(p->usearch, subject->ptr, subject->len, &status);
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "usearch_setText");
                return ENGINE_FAILURE;
            }
            for (l = usearch_first(p->usearch, &status); U_SUCCESS(status) && USEARCH_DONE != l; l = usearch_next(p->usearch, &status)) {
                matches++;
                u = l + usearch_getMatchedLength(p->usearch);
                if (interval_list_add(intervals, subject->len, l, u)) {
                    return ENGINE_WHOLE_LINE_MATCH;
                }
            }
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "usearch_[first|next]");
                return ENGINE_FAILURE;
            }
            usearch_unbindText(p->usearch);

            return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
        } else {
            return ENGINE_NO_MATCH;
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

static engine_return_t engine_fixed_whole_line_match(error_t **error, void *data, const UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    if (ustring_empty(p->pattern)) {
        return ustring_empty(subject) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH;
    } else if (NULL != p->usearch) {
        int32_t ret;
        UErrorCode status;

        status = U_ZERO_ERROR;
        usearch_setText(p->usearch, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_setText");
            return ENGINE_FAILURE;
        }
        ret = usearch_first(p->usearch, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_first");
            return ENGINE_FAILURE;
        }
        usearch_unbindText(p->usearch);

        return (ret != USEARCH_DONE && ((size_t) usearch_getMatchedLength(p->usearch)) == subject->len ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
    } else {
        if (IS_CASE_INSENSITIVE(p->flags)) {
            return (0 == u_strcasecmp(p->pattern->ptr, subject->ptr, 0) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
        } else {
            return (0 == u_strcmp(p->pattern->ptr, subject->ptr) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
        }
    }
}

static int32_t engine_fixed_split(error_t **error, void *data, const UString *subject, DPtrArray *array)
{
    UErrorCode status;
    int32_t l, u, pieces;
    FETCH_DATA(data, p, fixed_pattern_t);

    l = u = pieces = 0;
    status = U_ZERO_ERROR;
    if (NULL != p->usearch) {
        usearch_setText(p->usearch, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_setText");
            return ENGINE_FAILURE;
        }
        for (l = usearch_first(p->usearch, &status); U_SUCCESS(status) && USEARCH_DONE != l; l = usearch_next(p->usearch, &status)) {
            ++pieces;
            add_match(array, subject, l, u);
            u = l + usearch_getMatchedLength(p->usearch);
        }
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_[first|next]");
            return ENGINE_FAILURE;
        }
        usearch_unbindText(p->usearch);
    } else {
        UChar *m;

        ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_setText");
            return ENGINE_FAILURE;
        }
        while (NULL != (m = u_strFindFirst(subject->ptr + u, subject->len - u, p->pattern->ptr, p->pattern->len))) {
            u = m - subject->ptr;
            if (ubrk_isBoundary(p->ubrk, u) && ubrk_isBoundary(p->ubrk, u + p->pattern->len)) {
                ++pieces;
                add_match(array, subject, l, u);
            }
            l = u = u + p->pattern->len;
        }
        ubrk_unbindText(p->ubrk);
    }
    if (!pieces) {
//         add_match(array, subject, 0, subject->len);
//         ++pieces;
    } else if ((size_t) u < subject->len) {
        add_match(array, subject, u, subject->len);
        ++pieces;
    }

    return pieces;
}

static void engine_fixed_destroy(void *data)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    fixed_pattern_destroy(p);
}

engine_t fixed_engine = {
    engine_fixed_compile,
    engine_fixed_match,
    engine_fixed_match_all,
    engine_fixed_whole_line_match,
    engine_fixed_split,
    engine_fixed_destroy
};
