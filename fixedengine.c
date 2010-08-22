#include "ugrep.h"

extern UBool iFlag; // drop this

// TODO: switch from UChar * to UString * for upattern ?

static UChar *ustrndup(const UChar *src, int32_t length)
{
    UChar *dst;

    dst = mem_new_n(*dst, length + 1);
    u_strncpy(dst, src, length);

    return dst;
}

static void *engine_fixed_compute(const UChar *upattern, int32_t length)
{
    if (length < 0) {
        length = u_strlen(upattern);
    }

    return ustrndup(upattern, length);
}

static void *engine_fixed_computeC(const char *pattern)
{
    UChar *upattern;
    UConverter *ucnv;
    UErrorCode status;
    int32_t ulen, len, allocated;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu(status, "ucnv_open");
        return NULL;
    }
    len = strlen(pattern);
    allocated = len * ucnv_getMaxCharSize(ucnv) + 1;
    upattern = mem_new_n(*upattern, allocated);
    ulen = ucnv_toUChars(ucnv, upattern, allocated, pattern, len, &status);
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        free(upattern);
        return NULL;
    }

    return upattern;
}

static UBool engine_fixed_match(void *data, const UString *subject)
{
    FETCH_DATA(data, upattern, UChar);

    if (iFlag) {
        // Case Insensitive version
        return FALSE; // TODO
    } else {
        return (NULL != u_strFindFirst(subject->ptr, subject->len, upattern, -1)); // TODO: find better, inappropriate for binary file
    }
}

static UBool engine_fixed_whole_line_match(void *data, const UString *subject)
{
    FETCH_DATA(data, pattern, UChar);

    if (iFlag) {
        return (0 == u_strcasecmp(pattern, subject->ptr, 0));
    } else {
        return (0 == u_strcmp(pattern, subject->ptr));
    }
}

static void engine_fixed_reset(void *data)
{
    /* NOP */
    return;
}

static void engine_fixed_destroy(void *data)
{
    FETCH_DATA(data, upattern, UChar);

    free(upattern);
}

engine_t fixed_engine = {
    engine_fixed_compute,
    engine_fixed_computeC,
    engine_fixed_match,
    engine_fixed_whole_line_match,
    engine_fixed_reset,
    engine_fixed_destroy
};
