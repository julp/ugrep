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

static UBool engine_icure_match(void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
        // TODO
    }
    ret = uregex_find(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_find");
        // TODO
    }

    while (uregex_findNext(uregex, &status)) {
        printf("Match : [%d;%d]\n", uregex_start(uregex, 0, &status), uregex_end(uregex, 0, &status));
    }
    uregex_reset(uregex, 0, &status);

    return ret;
}

static UBool engine_icure_match_all(void *data, const UString *subject, slist_t *intervals)
{
    int matches;
    int32_t l, u;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    u_printf("%S\n", subject->ptr);
    matches = 0;
    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
        return FALSE;
    }
    while (uregex_findNext(uregex, &status)) {
        matches++;
        l = uregex_start(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu(status, "uregex_start");
            return FALSE;
        }
        u = uregex_end(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu(status, "uregex_end");
            return FALSE;
        }
        if (interval_add(intervals, subject->len, l, u)) {
            debug("whole line match"); //TODO: return significant value (UBool => int [-1: error, 0: normal, 1: whole line])
            break; // we already have a whole line matching, don't search anymore any pattern
        }
    }
    if (U_FAILURE(status)) {
        icu(status, "uregex_findNext");
        return FALSE;
    }
    uregex_reset(uregex, 0, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_reset");
        return FALSE;
    }

    return TRUE;
}

static UBool engine_icure_whole_line_match(void *data, const UString *subject)
{
    UBool ret;
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    status = U_ZERO_ERROR;
    uregex_setText(uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_setText");
        // TODO
    }
    ret = uregex_matches(uregex, -1, &status);
    if (U_FAILURE(status)) {
        icu(status, "uregex_matches");
        // TODO
    }

    return ret;
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
