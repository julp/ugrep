#include "ugrep.h"

extern UBool iFlag; // drop this

// TODO: switch from UChar * to UString * for upattern ?

static void engine_fixed_replace1(void *data, const UString *subject);

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

    engine_fixed_replace1(data, subject); // for test

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
}

static void engine_fixed_destroy(void *data)
{
    FETCH_DATA(data, upattern, UChar);

    free(upattern);
}

static void engine_fixed_replace1(void *data, const UString *subject)
{
    UChar *m;
    int32_t pos, upattern_len;
    FETCH_DATA(data, upattern, UChar);
    UChar after[] = {0x001b, 0x005b, 0x0030, 0x006d, U_NUL};
    UChar before[] = {0x001b, 0x005b, 0x0031, 0x003b, 0x0033, 0x0031, 0x006d, U_NUL};
    int32_t before_len = ARRAY_SIZE(before) - 1, after_len = ARRAY_SIZE(after) - 1;

    pos = 0;
    upattern_len = u_strlen(upattern);
    while (NULL != (m = u_strFindFirst(subject->ptr + pos, subject->len - pos, upattern, upattern_len))) {
        pos = m - subject->ptr;
        ustring_insert_len(subject, pos, before, before_len);
        ustring_insert_len(subject, pos + before_len + upattern_len, after, after_len);
        pos += before_len + upattern_len + after_len;
    }
}

engine_t fixed_engine = {
    engine_fixed_compute,
    engine_fixed_computeC,
    engine_fixed_match,
    engine_fixed_whole_line_match,
    engine_fixed_reset,
    engine_fixed_destroy
};
