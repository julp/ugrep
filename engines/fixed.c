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

static void *engine_fixed_compile(error_t **error, const UChar *upattern, int32_t length, uint32_t flags)
{
    UErrorCode status;
    fixed_pattern_t *p;

    p = mem_new(*p);
    p->pattern = ustring_dup_string_len(upattern, length); // not needed with usearch ?
    p->flags = flags;
    p->ubrk = NULL;
    p->usearch = NULL;
    status = U_ZERO_ERROR;
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
        p->usearch = usearch_open(upattern, length, USEARCH_FAKE_USTR, uloc_getDefault(), p->ubrk, &status);
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

    return p;
}

static void *engine_fixed_compileC(error_t **error, const char *pattern, uint32_t flags)
{
    int32_t len;
    UErrorCode status;
    fixed_pattern_t *p;

    len = strlen(pattern);
    p = mem_new(*p);
    p->flags = flags;
    p->ubrk = NULL;
    p->usearch = NULL;
    status = U_ZERO_ERROR;
    if (NULL == (p->pattern = ustring_convert_argv_from_local(pattern, error))) {
        fixed_pattern_destroy(p);
        return NULL;
    }
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
        p->usearch = usearch_open(p->pattern->ptr, p->pattern->len, USEARCH_FAKE_USTR, uloc_getDefault(), p->ubrk, &status);
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

    return p;
}

static engine_return_t engine_fixed_match(error_t **error, void *data, const UString *subject)
{
    int32_t ret;
    UErrorCode status;
    FETCH_DATA(data, p, fixed_pattern_t);

    status = U_ZERO_ERROR;
    if (NULL != p->usearch) {
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
            usearch_setText(p->usearch, USEARCH_FAKE_USTR, &status);
            assert(U_SUCCESS(status));

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
        ubrk_setText(p->ubrk, NULL, 0, &status);
        assert(U_SUCCESS(status));

        return ret;
    }
}

#ifdef OLD_INTERVAL
static engine_return_t engine_fixed_match_all(error_t **error, void *data, const UString *subject, slist_t *intervals)
#else
static engine_return_t engine_fixed_match_all(error_t **error, void *data, const UString *subject, slist_pool_t *intervals)
#endif /* OLD_INTERVAL */
{
    int32_t matches;
    UErrorCode status;
    FETCH_DATA(data, p, fixed_pattern_t);

    matches = 0;
    status = U_ZERO_ERROR;
    if (NULL != p->usearch) {
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
                if (interval_add(intervals, subject->len, l, u)) {
                    return ENGINE_WHOLE_LINE_MATCH;
                }
            }
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "usearch_[first|next]");
                return ENGINE_FAILURE;
            }
            usearch_setText(p->usearch, USEARCH_FAKE_USTR, &status);
            assert(U_SUCCESS(status));

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
                if (interval_add(intervals, subject->len, pos, pos + p->pattern->len)) {
                    return ENGINE_WHOLE_LINE_MATCH;
                }
            }
            pos += p->pattern->len;
        }
        ubrk_setText(p->ubrk, NULL, 0, &status);
        assert(U_SUCCESS(status));

        return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
    }
}

static engine_return_t engine_fixed_whole_line_match(error_t **error, void *data, const UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    if (NULL != p->usearch) {
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
        usearch_setText(p->usearch, USEARCH_FAKE_USTR, &status);
        assert(U_SUCCESS(status));

        return (ret != USEARCH_DONE && ((size_t) usearch_getMatchedLength(p->usearch)) == subject->len ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
    } else {
        if (IS_CASE_INSENSITIVE(p->flags)) {
            return (0 == u_strcasecmp(p->pattern->ptr, subject->ptr, 0) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
        } else {
            return (0 == u_strcmp(p->pattern->ptr, subject->ptr) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
        }
    }
}

static void engine_fixed_destroy(void *data)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    fixed_pattern_destroy(p);
}

engine_t fixed_engine = {
    engine_fixed_compile,
    engine_fixed_compileC,
    engine_fixed_match,
    engine_fixed_match_all,
    engine_fixed_whole_line_match,
    engine_fixed_destroy
};
