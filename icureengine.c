#include "ugrep.h"

extern UBool iFlag; // drop this

static void *engine_icure_compute(const UChar *upattern, int32_t length)
{
    UFILE *ustderr;
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    ustderr = u_finit(stdout, NULL, NULL);
    /* don't make a copy of upattern, ICU does this */
    uregex = uregex_open(upattern, length, iFlag ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
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

static void *engine_icure_computeC(const char *pattern)
{
    UParseError pe;
    UErrorCode status;
    URegularExpression *uregex;

    status = U_ZERO_ERROR;
    uregex = uregex_openC(pattern, iFlag ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
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

    return ret;
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
    UErrorCode status;
    FETCH_DATA(data, uregex, URegularExpression);

    uregex_close(uregex);
}

engine_t icure_engine = {
    engine_icure_compute,
    engine_icure_computeC,
    engine_icure_match,
    engine_icure_whole_line_match,
    engine_icure_reset,
    engine_icure_destroy
};
