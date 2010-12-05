#include "ugrep.h"

static UChar EMPTY_STR[] = { 0, 0 };

static void *engine_icusearch_compile(error_t **error, const UChar *upattern, int32_t length, UBool case_insensitive, UBool word_bounded)
{
    UCollator *ucol;
    UErrorCode status;
    UBreakIterator *ubrk;
    UStringSearch *usearch;

    ubrk = NULL;
    status = U_ZERO_ERROR;
    if (word_bounded) {
        ubrk = ubrk_open(UBRK_WORD, uloc_getDefault(), EMPTY_STR, 1, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_open");
            return NULL;
        }
    }
    ucol = ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucol_open");
        return NULL;
    }
    if (case_insensitive) {
        ucol_setAttribute(ucol, UCOL_STRENGTH, UCOL_PRIMARY, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ucol_setAttribute");
            return NULL;
        }
        ucol_setAttribute(ucol, UCOL_CASE_LEVEL, UCOL_ON, &status); /* TODO: set it even if case_insensitive == FALSE to ignore accents? */
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ucol_setAttribute");
            return NULL;
        }
    }
    usearch = usearch_openFromCollator(upattern, length, EMPTY_STR, 1, ucol, ubrk, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "usearch_openFromCollator");
        return NULL;
    }

    return usearch;
}

static void *engine_icusearch_compileC(error_t **error, const char *pattern, UBool case_insensitive, UBool word_bounded)
{
    int32_t len;
    UCollator *ucol;
    UConverter *ucnv;
    UErrorCode status;
    UString *upattern;
    UBreakIterator *ubrk;
    UStringSearch *usearch;

    /*
     * TODO: upattern never freed? Use Uchar*?
     */
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
        return NULL;
    }
    len = strlen(pattern);
    upattern = ustring_sized_new(len * ucnv_getMaxCharSize(ucnv) + 1);
    upattern->len = ucnv_toUChars(ucnv, upattern->ptr, upattern->allocated, pattern, len, &status);
    upattern->ptr[upattern->len] = U_NUL;
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_toUChars");
        ustring_destroy(upattern);
        return NULL;
    }

    ubrk = NULL;
    status = U_ZERO_ERROR;
    if (word_bounded) {
        ubrk = ubrk_open(UBRK_WORD, uloc_getDefault(), EMPTY_STR, 1, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_open");
            ustring_destroy(upattern);
            return NULL;
        }
    }
    ucol = ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucol_open");
        // TODO: free ubrk ?
        ustring_destroy(upattern);
        return NULL;
    }
    if (case_insensitive) {
        ucol_setAttribute(ucol, UCOL_STRENGTH, UCOL_PRIMARY, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ucol_setAttribute");
            // TODO: free ubrk ?
            ustring_destroy(upattern);
            return NULL;
        }
        ucol_setAttribute(ucol, UCOL_CASE_LEVEL, UCOL_ON, &status); /* TODO: set it even if case_insensitive == FALSE to ignore accents? */
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ucol_setAttribute");
            // TODO: free ubrk ?
            ustring_destroy(upattern);
            return NULL;
        }
    }
    usearch = usearch_openFromCollator(upattern->ptr, upattern->len, EMPTY_STR, 1, ucol, ubrk, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "usearch_openFromCollator");
        // TODO: free ubrk + ucol ?
        ustring_destroy(upattern);
        return NULL;
    }

    return usearch;
}

static engine_return_t engine_icusearch_match(error_t **error, void *data, const UString *subject)
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
static engine_return_t engine_icusearch_match_all(error_t **error, void *data, const UString *subject, slist_t *intervals)
#else
static engine_return_t engine_icusearch_match_all(error_t **error, void *data, const UString *subject, slist_pool_t *intervals)
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
            icu_error_set(error, FATAL, status, "usearch_[first|next]"/*(matches == 0 ? "usearch_first" : "usearch_next")*/);
            return ENGINE_FAILURE;
        }
        /*usearch_reset(usearch, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "usearch_reset");
            return ENGINE_FAILURE;
        }*/

        return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
    } else {
        return ENGINE_NO_MATCH;
    }
}

static engine_return_t engine_icusearch_whole_line_match(error_t **error, void *data, const UString *subject)
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

    // TODO: unsafe because of case, the length could be different?
    return (ret != USEARCH_DONE && usearch_getMatchedLength(usearch) == subject->len ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
}

static void engine_icusearch_destroy(void *data)
{
    FETCH_DATA(data, usearch, UStringSearch);

    // TODO: are associated UBreakIterator and UCollator destroyed too ?
    // ucol_close(usearch_getCollator(usearch));
    // ubrk_close(usearch_getBreakIterator(usearch));
    usearch_close(usearch);
}

engine_t icusearch_engine = {
    engine_icusearch_compile,
    engine_icusearch_compileC,
    engine_icusearch_match,
    engine_icusearch_match_all,
    engine_icusearch_whole_line_match,
    engine_icusearch_destroy
};
