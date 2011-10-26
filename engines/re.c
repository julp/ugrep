#include "engine.h"

#include <unicode/ubrk.h>
#include <unicode/uregex.h>

// const UChar b[] = {0x005c, 0x0062, U_NUL};

static const UChar _UREGEXP_FAKE_USTR[] = { 0 };
#define UREGEXP_FAKE_USTR _UREGEXP_FAKE_USTR, 0

typedef struct {
    UBreakIterator *ubrk;
    URegularExpression *uregex;
} re_pattern_t;

static void re_pattern_reset(re_pattern_t *p)
{
    UErrorCode status;

    status = U_ZERO_ERROR;
    ubrk_setText(p->ubrk, NULL, 0, &status);
    assert(U_SUCCESS(status));
    uregex_setText(p->uregex, UREGEXP_FAKE_USTR, &status);
    assert(U_SUCCESS(status));
}

static void re_pattern_destroy(re_pattern_t *p)
{
    if (NULL != p->uregex) {
        uregex_close(p->uregex);
    }
    if (NULL != p->ubrk) {
        ubrk_close(p->ubrk);
    }
    free(p);
}

static void *engine_re_compile(error_t **error, const UChar *upattern, int32_t length, uint32_t flags)
{
    re_pattern_t *p;
    UErrorCode status;
    UParseError pe = { -1, -1, {0}, {0} };

    status = U_ZERO_ERROR;
    p = mem_new(*p);
    p->ubrk = NULL;
    /* don't make a copy of upattern, ICU does this */
    p->uregex = uregex_open(upattern, length, IS_CASE_INSENSITIVE(flags) ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    if (U_FAILURE(status)) {
        if (-1 != pe.line) {
            error_set(error, FATAL, "Invalid pattern: error at offset %d\n\t%S\n\t%*c\n", pe.offset, upattern, pe.offset, '^');
        } else {
            icu_error_set(error, FATAL, status, "uregex_open");
        }
        return NULL;
    }
    p->ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
    if (U_FAILURE(status)) {
        re_pattern_destroy(p);
        icu_error_set(error, FATAL, status, "ubrk_open");
        return NULL;
    }

    return p;
}

static void *engine_re_compileC(error_t **error, const char *pattern, uint32_t flags)
{
    re_pattern_t *p;
    UErrorCode status;
    UParseError pe = { -1, -1, {0}, {0} };

    status = U_ZERO_ERROR;
    p = mem_new(*p);
    p->ubrk = NULL;
    p->uregex = uregex_openC(pattern, IS_CASE_INSENSITIVE(flags) ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    if (U_FAILURE(status)) {
        if (-1 != pe.line) {
            error_set(error, FATAL, "Invalid pattern: error at offset %d\n\t%s\n\t%*c\n", pe.offset, pattern, pe.offset, '^');
        } else {
            icu_error_set(error, FATAL, status, "uregex_openC");
        }
        return NULL;
    }
    p->ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
    if (U_FAILURE(status)) {
        re_pattern_destroy(p);
        icu_error_set(error, FATAL, status, "ubrk_open");
        return NULL;
    }

    return p;
}

static engine_return_t engine_re_match(error_t **error, void *data, const UString *subject)
{
    UBool ret;
    int32_t l, u;
    UErrorCode status;
    FETCH_DATA(data, p, re_pattern_t);

    status = U_ZERO_ERROR;
    uregex_setText(p->uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ret = uregex_find(p->uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_find");
        return ENGINE_FAILURE;
    }
    if (ret) {
        l = uregex_start(p->uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_start");
            return ENGINE_FAILURE;
        }
        u = uregex_end(p->uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_start");
            return ENGINE_FAILURE;
        }
        ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_setText");
            return ENGINE_FAILURE;
        }
        ret = ubrk_isBoundary(p->ubrk, l) && ubrk_isBoundary(p->ubrk, u);
    }
    re_pattern_reset(p);

    return (ret ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

#ifdef OLD_INTERVAL
static engine_return_t engine_re_match_all(error_t **error, void *data, const UString *subject, slist_t *intervals)
#else
static engine_return_t engine_re_match_all(error_t **error, void *data, const UString *subject, slist_pool_t *intervals)
#endif /* OLD_INTERVAL */
{
    int matches;
    int32_t l, u;
    UErrorCode status;
    FETCH_DATA(data, p, re_pattern_t);

    matches = 0;
    status = U_ZERO_ERROR;
    uregex_setText(p->uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ubrk_setText");
        return ENGINE_FAILURE;
    }
    while (uregex_findNext(p->uregex, &status)) {
        l = uregex_start(p->uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_start");
            return ENGINE_FAILURE;
        }
        u = uregex_end(p->uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_end");
            return ENGINE_FAILURE;
        }
        if (ubrk_isBoundary(p->ubrk, l) && ubrk_isBoundary(p->ubrk, u)) {
            matches++;
            if (interval_add(intervals, subject->len, l, u)) {
                return ENGINE_WHOLE_LINE_MATCH;
            }
        }
    }
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_findNext");
        return ENGINE_FAILURE;
    }
    re_pattern_reset(p);

    return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

static engine_return_t engine_re_whole_line_match(error_t **error, void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, p, re_pattern_t);

    status = U_ZERO_ERROR;
    uregex_setText(p->uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ret = uregex_matches(p->uregex, -1, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_matches");
        return ENGINE_FAILURE;
    }
    re_pattern_reset(p);

    return (ret ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
}

static void engine_re_destroy(void *data)
{
    FETCH_DATA(data, p, re_pattern_t);

    re_pattern_destroy(p);
}

engine_t re_engine = {
    engine_re_compile,
    engine_re_compileC,
    engine_re_match,
    engine_re_match_all,
    engine_re_whole_line_match,
    engine_re_destroy
};
