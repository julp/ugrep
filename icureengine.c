#include "ugrep.h"

static void *engine_icure_compile(const UChar *upattern, int32_t length, UBool case_insensitive)
{
    UFILE *ustderr;
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    ustderr = u_finit(stdout, NULL, NULL);
    /* don't make a copy of upattern, ICU does this */
    uregex = uregex_open(upattern, length, case_insensitive ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    if (U_FAILURE(status)) {
        if (U_REGEX_RULE_SYNTAX == status) {
            //u_fprintf(ustderr, "Error at offset %d %S %S\n", pe.offset, pe.preContext, pe.postContext);
            u_fprintf(ustderr, "Invalid pattern: error at offset %d\n", pe.offset);
            u_fprintf(ustderr, "\t%S\n", upattern);
            u_fprintf(ustderr, "\t%*c\n", pe.offset, '^');
        } else {
            icu(status, "uregex_openC");
        }
        return NULL;
    }

    return uregex;
}

static void *engine_icure_compileC(const char *pattern, UBool case_insensitive)
{
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    uregex = uregex_openC(pattern, case_insensitive ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    if (U_FAILURE(status)) {
        if (U_REGEX_RULE_SYNTAX == status) {
            //u_fprintf(ustderr, "Error at offset %d %S %S\n", pe.offset, pe.preContext, pe.postContext);
            fprintf(stderr, "Invalid pattern: error at offset %d\n", pe.offset);
            fprintf(stderr, "\t%s\n", pattern);
            fprintf(stderr, "\t%*c\n", pe.offset, '^');
        } else {
            icu(status, "uregex_openC");
        }
        return NULL;
    }

    return uregex;
}

static void engine_icure_pre_exec(void *UNUSED(data), UString *UNUSED(subject))
{
    /* NOP */
}

static engine_return_t engine_icure_match(void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ret = uregex_find(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_find");
        return ENGINE_FAILURE;
    }

    return (ret ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

static engine_return_t engine_icure_match_all(void *data, const UString *subject, slist_t *intervals)
{
    int matches;
    int32_t l, u;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    matches = 0;
    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    while (uregex_findNext(uregex, &status)) {
        matches++;
        l = uregex_start(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu(status, "uregex_start");
            return ENGINE_FAILURE;
        }
        u = uregex_end(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu(status, "uregex_end");
            return ENGINE_FAILURE;
        }
        if (interval_add(intervals, subject->len, l, u)) {
            return ENGINE_WHOLE_LINE_MATCH;
        }
    }
    if (U_FAILURE(status)) {
        icu(status, "uregex_findNext");
        return ENGINE_FAILURE;
    }
    /*uregex_reset(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_reset");
        return ENGINE_FAILURE;
    }*/

    return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

static engine_return_t engine_icure_whole_line_match(void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
        return ENGINE_FAILURE;
    }
    ret = uregex_matches(uregex, -1, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_matches");
        return ENGINE_FAILURE;
    }

    return (ret ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
}

static void engine_icure_reset(void *data)
{
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, 0, 0, &status);
}

static void engine_icure_destroy(void *data)
{
    FETCH_DATA(data, uregex, URegularExpression);

    uregex_close(uregex);
}

engine_t icure_engine = {
    engine_icure_compile,
    engine_icure_compileC,
    engine_icure_pre_exec,
    engine_icure_match,
    engine_icure_match_all,
    engine_icure_whole_line_match,
    engine_icure_reset,
    engine_icure_destroy
};
