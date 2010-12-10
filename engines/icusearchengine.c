#include "ugrep.h"

static UChar _USEARCH_FAKE_USTR[] = { 0, 0 };
#define USEARCH_FAKE_USTR _USEARCH_FAKE_USTR, 1 // empty stings refused by usearch

static void *engine_search_compile(error_t **error, const UChar *upattern, int32_t length, uint32_t flags)
{
    UCollator *ucol;
    UErrorCode status;
    UBreakIterator *ubrk;
    UStringSearch *usearch;
    const char *inherited_loc;

    ubrk = NULL;
    status = U_ZERO_ERROR;
    inherited_loc = uloc_getDefault();
    if (word_bounded) {
        ubrk = ubrk_open(UBRK_WORD, inherited_loc, USEARCH_FAKE_USTR, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_open");
            return NULL;
        }
    }
    ucol = ucol_open(inherited_loc, &status);
    if (U_FAILURE(status)) {
        if (NULL != ubrk) {
            ubrk_close(ubrk);
        }
        icu_error_set(error, FATAL, status, "ucol_open");
        return NULL;
    }
    if (case_insensitive) {
        ucol_setStrength(ucol, UCOL_PRIMARY);
    }
    usearch = usearch_openFromCollator(upattern, length, USEARCH_FAKE_USTR, ucol, ubrk, &status);
    if (U_FAILURE(status)) {
        if (NULL != ubrk) {
            ubrk_close(ubrk);
        }
        ucol_close(ucol);
        icu_error_set(error, FATAL, status, "usearch_openFromCollator");
        return NULL;
    }

    return usearch;
}

static void *engine_search_compileC(error_t **error, const char *pattern, uint32_t flags)
{
    int32_t len;
    UCollator *ucol;
    UChar *upattern;
    UConverter *ucnv;
    UErrorCode status;
    int32_t upattern_len;
    UBreakIterator *ubrk;
    UStringSearch *usearch;
    const char *inherited_loc;
    int32_t upattern_allocated;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
        return NULL;
    }
    len = strlen(pattern);
    upattern_allocated = len * ucnv_getMaxCharSize(ucnv) + 1;
    upattern = mem_new_n(UChar, upattern_allocated);
    upattern_len = ucnv_toUChars(ucnv, upattern, upattern_allocated, pattern, len, &status);
    upattern[upattern_len] = U_NUL;
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_toUChars");
        free(upattern);
        return NULL;
    }
    ubrk = NULL;
    inherited_loc = uloc_getDefault();
    if (word_bounded) {
        ubrk = ubrk_open(UBRK_WORD, inherited_loc, USEARCH_FAKE_USTR, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_open");
            free(upattern);
            return NULL;
        }
    }
    ucol = ucol_open(inherited_loc, &status);
    if (U_FAILURE(status)) {
        if (NULL != ubrk) {
            ubrk_close(ubrk);
        }
        icu_error_set(error, FATAL, status, "ucol_open");
        free(upattern);
        return NULL;
    }
    if (case_insensitive) {
        ucol_setStrength(ucol, UCOL_PRIMARY);
    }
    usearch = usearch_openFromCollator(upattern, upattern_len, USEARCH_FAKE_USTR, ucol, ubrk, &status);
    if (U_FAILURE(status)) {
        if (NULL != ubrk) {
            ubrk_close(ubrk);
        }
        ucol_close(ucol);
        icu_error_set(error, FATAL, status, "usearch_openFromCollator");
        free(upattern);
        return NULL;
    }

    return usearch;
}

static engine_return_t engine_search_match(error_t **error, void *data, const UString *subject)
{
    int32_t ret;
    UErrorCode status;
    FETCH_DATA(data, usearch, UStringSearch);

    status = U_ZERO_ERROR;
    if (subject->len > 0) {
        usearch_setText(usearch, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_setText");
            return ENGINE_FAILURE;
        }
        ret = usearch_first(usearch, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_first");
            return ENGINE_FAILURE;
        }

        return (ret != USEARCH_DONE ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
    } else {
        return ENGINE_NO_MATCH;
    }
}

#ifdef OLD_INTERVAL
static engine_return_t engine_search_match_all(error_t **error, void *data, const UString *subject, slist_t *intervals)
#else
static engine_return_t engine_search_match_all(error_t **error, void *data, const UString *subject, slist_pool_t *intervals)
#endif /* OLD_INTERVAL */
{
    int matches;
    int32_t l, u;
    UErrorCode status;
    FETCH_DATA(data, usearch, UStringSearch);

    matches = 0;
    status = U_ZERO_ERROR;
    if (subject->len > 0) {
        usearch_setText(usearch, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_setText");
            return ENGINE_FAILURE;
        }
        for (l = usearch_first(usearch, &status); U_SUCCESS(status) && USEARCH_DONE != l; l = usearch_next(usearch, &status)) {
            matches++;
            u = l + usearch_getMatchedLength(usearch);
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
}

static engine_return_t engine_search_whole_line_match(error_t **error, void *data, const UString *subject)
{
    int32_t ret;
    UErrorCode status;
    FETCH_DATA(data, usearch, UStringSearch);

    status = U_ZERO_ERROR;
    usearch_setText(usearch, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "usearch_setText");
        return ENGINE_FAILURE;
    }
    ret = usearch_first(usearch, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "usearch_first");
        return ENGINE_FAILURE;
    }

    // TODO: is it safe ? (because of case, the length could be different)
    return (ret != USEARCH_DONE && usearch_getMatchedLength(usearch) == subject->len ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
}

static void engine_search_destroy(void *data)
{
    FETCH_DATA(data, usearch, UStringSearch);

    // TODO: free pattern?
    ucol_close(usearch_getCollator(usearch));
    // ubrk_close(usearch_getBreakIterator(usearch)); // done by usearch_close
    usearch_close(usearch);
}

engine_t search_engine = {
    engine_search_compile,
    engine_search_compileC,
    engine_search_match,
    engine_search_match_all,
    engine_search_whole_line_match,
    engine_search_destroy
};
