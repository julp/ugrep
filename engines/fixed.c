#include "ugrep.h"

static UChar _USEARCH_FAKE_USTR[] = { 0, 0 };
#define USEARCH_FAKE_USTR _USEARCH_FAKE_USTR, 1 // empty stings refused by usearch

typedef struct {
    uint32_t flags;
    UString *pattern;
    UStringSearch *usearch;
} fixed_pattern_t;

static void pattern_destroy(fixed_pattern_t *p)
{
    if (NULL != p->usearch) {
        ucol_close(usearch_getCollator(p->usearch));
        // ubrk_close(usearch_getBreakIterator(p->usearch)); // done by usearch_close
        usearch_close(p->usearch);
    }
    ustring_destroy(p->pattern);
    free(p);
}

static void *engine_fixed_compile(error_t **error, const UChar *upattern, int32_t length, uint32_t flags)
{
    fixed_pattern_t *p;

    p = mem_new(*p);
    p->pattern = ustring_dup_string_len(upattern, length); // not needed with usearch ?
    p->flags = flags;
    p->usearch = NULL;
    if (IS_WORD_BOUNDED(flags) || (IS_CASE_INSENSITIVE(flags) && !IS_WHOLE_LINE(flags))) {
        UCollator *ucol;
        UErrorCode status;
        UBreakIterator *ubrk;
        const char *inherited_loc;

        ubrk = NULL;
        status = U_ZERO_ERROR;
        inherited_loc = uloc_getDefault();
        if (IS_WORD_BOUNDED(flags)) {
            ubrk = ubrk_open(UBRK_WORD, inherited_loc, USEARCH_FAKE_USTR, &status);
            if (U_FAILURE(status)) {
                pattern_destroy(p);
                icu_error_set(error, FATAL, status, "ubrk_open");
                return NULL;
            }
        }
        ucol = ucol_open(inherited_loc, &status);
        if (U_FAILURE(status)) {
            if (NULL != ubrk) {
                ubrk_close(ubrk);
            }
            pattern_destroy(p);
            icu_error_set(error, FATAL, status, "ucol_open");
            return NULL;
        }
        if (IS_CASE_INSENSITIVE(flags)) {
            ucol_setStrength(ucol, UCOL_PRIMARY);
        }
        p->usearch = usearch_openFromCollator(upattern, length, USEARCH_FAKE_USTR, ucol, ubrk, &status);
        if (U_FAILURE(status)) {
            if (NULL != ubrk) {
                ubrk_close(ubrk);
            }
            ucol_close(ucol);
            pattern_destroy(p);
            icu_error_set(error, FATAL, status, "usearch_openFromCollator");
            return NULL;
        }
    }

    return p;
}

static void *engine_fixed_compileC(error_t **error, const char *pattern, uint32_t flags)
{
    int32_t len;
    UConverter *ucnv;
    UErrorCode status;
    fixed_pattern_t *p;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
        return NULL;
    }
    len = strlen(pattern);
    p = mem_new(*p);
    p->flags = flags;
    p->usearch = NULL;
    p->pattern = ustring_sized_new(len * ucnv_getMaxCharSize(ucnv) + 1);
    p->pattern->len = ucnv_toUChars(ucnv, p->pattern->ptr, p->pattern->allocated, pattern, len, &status);
    p->pattern->ptr[p->pattern->len] = U_NUL;
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        pattern_destroy(p);
        icu_error_set(error, FATAL, status, "ucnv_toUChars");
        return NULL;
    }
    if (IS_WORD_BOUNDED(flags) || (IS_CASE_INSENSITIVE(flags) && !IS_WHOLE_LINE(flags))) {
        UCollator *ucol;
        UBreakIterator *ubrk;
        const char *inherited_loc;

        debug("SEARCH");
        ubrk = NULL;
        inherited_loc = uloc_getDefault();
        if (IS_WORD_BOUNDED(flags)) {
            ubrk = ubrk_open(UBRK_WORD, inherited_loc, USEARCH_FAKE_USTR, &status);
            if (U_FAILURE(status)) {
                pattern_destroy(p);
                icu_error_set(error, FATAL, status, "ubrk_open");
                return NULL;
            }
        }
        ucol = ucol_open(inherited_loc, &status);
        if (U_FAILURE(status)) {
            if (NULL != ubrk) {
                ubrk_close(ubrk);
            }
            pattern_destroy(p);
            icu_error_set(error, FATAL, status, "ucol_open");
            return NULL;
        }
        if (IS_CASE_INSENSITIVE(flags)) {
            ucol_setStrength(ucol, UCOL_PRIMARY);
        }
        p->usearch = usearch_openFromCollator(p->pattern->ptr, p->pattern->len, USEARCH_FAKE_USTR, ucol, ubrk, &status);
        if (U_FAILURE(status)) {
            if (NULL != ubrk) {
                ubrk_close(ubrk);
            }
            ucol_close(ucol);
            pattern_destroy(p);
            icu_error_set(error, FATAL, status, "usearch_openFromCollator");
            return NULL;
        }
    } else {
        debug("FIXED");
    }

    return p;
}

static engine_return_t engine_fixed_match(error_t **error, void *data, const UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    if (NULL != p->usearch) {
        int32_t ret;
        UErrorCode status;

        status = U_ZERO_ERROR;
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

            return (ret != USEARCH_DONE ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
        } else {
            return ENGINE_NO_MATCH;
        }
    } else {
        return (NULL != u_strFindFirst(subject->ptr, subject->len, p->pattern->ptr, p->pattern->len) ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
    }
}

#ifdef OLD_INTERVAL
static engine_return_t engine_fixed_match_all(error_t **error, void *data, const UString *subject, slist_t *intervals)
#else
static engine_return_t engine_fixed_match_all(error_t **error, void *data, const UString *subject, slist_pool_t *intervals)
#endif /* OLD_INTERVAL */
{
    int32_t matches;
    FETCH_DATA(data, p, fixed_pattern_t);

    if (NULL != p->usearch) {
        int32_t l, u;
        UErrorCode status;

        matches = 0;
        status = U_ZERO_ERROR;
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

            return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
        } else {
            return ENGINE_NO_MATCH;
        }
    } else {
        UChar *m;
        int32_t pos;

        matches = pos = 0;
        while (NULL != (m = u_strFindFirst(subject->ptr + pos, subject->len - pos, p->pattern->ptr, p->pattern->len))) {
            pos = m - subject->ptr;
            matches++;
            if (interval_add(intervals, subject->len, pos, pos + p->pattern->len)) {
                return ENGINE_WHOLE_LINE_MATCH;
            }
            pos += p->pattern->len;
        }

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

        // TODO: is it safe ? (because of case, the length could be different)
        return (ret != USEARCH_DONE && usearch_getMatchedLength(p->usearch) == subject->len ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
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

    pattern_destroy(p);
}

engine_t fixed_engine = {
    engine_fixed_compile,
    engine_fixed_compileC,
    engine_fixed_match,
    engine_fixed_match_all,
    engine_fixed_whole_line_match,
    engine_fixed_destroy
};