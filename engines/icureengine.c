#include "ugrep.h"

// const UChar b[] = {0x005c, 0x0062, U_NUL};

static void *engine_icure_compile(error_t **error, const UChar *upattern, int32_t length, UBool case_insensitive, UBool word_bounded)
{
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    /* don't make a copy of upattern, ICU does this */
    uregex = uregex_open(upattern, length, case_insensitive ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    if (U_FAILURE(status)) {
        if (U_REGEX_RULE_SYNTAX == status) {
            error_set(error, FATAL, "Invalid pattern: error at offset %d\n\t%S\n\t%*c\n", pe.offset, upattern, pe.offset, '^');
        } else {
            icu_error_set(error, FATAL, status, "uregex_open");
        }
        return NULL;
    }

    return uregex;
}

static void *engine_icure_compileC(error_t **error, const char *pattern, UBool case_insensitive, UBool word_bounded)
{
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    uregex = uregex_openC(pattern, case_insensitive ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    if (U_FAILURE(status)) {
        if (U_REGEX_RULE_SYNTAX == status) {
            //u_fprintf(ustderr, "Error at offset %d %S %S\n", pe.offset, pe.preContext, pe.postContext);
            /*fprintf(stderr, "Invalid pattern: error at offset %d\n", pe.offset);
            fprintf(stderr, "\t%s\n", pattern);
            fprintf(stderr, "\t%*c\n", pe.offset, '^');*/
            error_set(error, FATAL, "Invalid pattern: error at offset %d\n\t%s\n\t%*c\n", pe.offset, pattern, pe.offset, '^');
        } else {
            icu_error_set(error, FATAL, status, "uregex_openC");
        }
        return NULL;
    }

    return uregex;
}

static engine_return_t engine_icure_match(error_t **error, void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ret = uregex_find(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_find");
        return ENGINE_FAILURE;
    }

    return (ret ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

#ifdef OLD_INTERVAL
static engine_return_t engine_icure_match_all(error_t **error, void *data, const UString *subject, slist_t *intervals)
#else
static engine_return_t engine_icure_match_all(error_t **error, void *data, const UString *subject, slist_pool_t *intervals)
#endif /* OLD_INTERVAL */
{
    int matches;
    int32_t l, u;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    matches = 0;
    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    while (uregex_findNext(uregex, &status)) {
        matches++;
        l = uregex_start(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_start");
            return ENGINE_FAILURE;
        }
        u = uregex_end(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_end");
            return ENGINE_FAILURE;
        }
        if (interval_add(intervals, subject->len, l, u)) {
            return ENGINE_WHOLE_LINE_MATCH;
        }
    }
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_findNext");
        return ENGINE_FAILURE;
    }
    /*uregex_reset(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_reset");
        return ENGINE_FAILURE;
    }*/

    return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

static engine_return_t engine_icure_whole_line_match(error_t **error, void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ret = uregex_matches(uregex, -1, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_matches");
        return ENGINE_FAILURE;
    }

    return (ret ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
}

static void engine_icure_destroy(void *data)
{
    FETCH_DATA(data, uregex, URegularExpression);

    uregex_close(uregex);
}

engine_t icure_engine = {
    engine_icure_compile,
    engine_icure_compileC,
    engine_icure_match,
    engine_icure_match_all,
    engine_icure_whole_line_match,
    engine_icure_destroy
};
