#include "ugrep.h"

typedef struct {
    UString *pattern;
    UBool case_insensitive;
} fixed_pattern_t;

/*static UChar *ustrndup(const UChar *src, int32_t length)
{
    UChar *dst;

    dst = mem_new_n(*dst, length + 1);
    u_strncpy(dst, src, length);

    return dst;
}*/

static void pattern_destroy(fixed_pattern_t *p)
{
    ustring_destroy(p->pattern);
    free(p);
}

static void *engine_fixed_compile(const UChar *upattern, int32_t length, UBool case_insensitive)
{
    /*if (length < 0) {
        length = u_strlen(upattern);
    }

    return ustrndup(upattern, length);*/
    fixed_pattern_t *p;

    p = mem_new(*p);
    p->pattern = ustring_dup_string_len(upattern, length);
    p->case_insensitive = case_insensitive;

    return p;
}

static void *engine_fixed_compileC(const char *pattern, UBool case_insensitive)
{
    //UChar *upattern;
    UConverter *ucnv;
    UErrorCode status;
    fixed_pattern_t *p;
    int32_t /*ulen, */len/*, allocated*/;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu(status, "ucnv_open");
        return NULL;
    }
    len = strlen(pattern);
    p = mem_new(*p);
    p->case_insensitive = case_insensitive;
    p->pattern = ustring_sized_new(len * ucnv_getMaxCharSize(ucnv) + 1);
    //allocated = len * ucnv_getMaxCharSize(ucnv) + 1;
    //upattern = mem_new_n(*upattern, allocated);
    //ulen = ucnv_toUChars(ucnv, upattern, allocated, pattern, len, &status);
    p->pattern->len = ucnv_toUChars(ucnv, p->pattern->ptr, p->pattern->allocated, pattern, len, &status);
    p->pattern->ptr[p->pattern->len] = U_NUL;
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        //free(upattern);
        pattern_destroy(p);
        return NULL;
    }
    if (case_insensitive) {
        if (!ustring_tolower(p->pattern)) {
            //free(upattern);
            pattern_destroy(p);
            return NULL;
        }
    }

    //return upattern;
    return p;
}

static void engine_fixed_pre_exec(void *data, UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    if (p->case_insensitive) {
        ustring_tolower(subject); // TODO
    }
}

static UBool engine_fixed_match(void *data, const UString *subject)
{
    /*FETCH_DATA(data, upattern, UChar);

    if (iFlag) {
        // Case Insensitive version
        return FALSE; // TODO
    } else {
        return (NULL != u_strFindFirst(subject->ptr, subject->len, upattern, -1)); // TODO: find better, inappropriate for binary file
    }*/
    FETCH_DATA(data, p, fixed_pattern_t);

    return (NULL != u_strFindFirst(subject->ptr, subject->len, p->pattern->ptr, p->pattern->len)); // TODO: find better, inappropriate for binary file
}

static UBool engine_fixed_whole_line_match(void *data, const UString *subject)
{
    /*FETCH_DATA(data, pattern, UChar);

    if (iFlag) {
        return (0 == u_strcasecmp(pattern, subject->ptr, 0));
    } else {
        return (0 == u_strcmp(pattern, subject->ptr));
    }*/
    FETCH_DATA(data, p, fixed_pattern_t);

    if (p->case_insensitive) {
        return (0 == u_strcasecmp(p->pattern->ptr, subject->ptr, 0));
    } else {
        return (0 == u_strcmp(p->pattern->ptr, subject->ptr));
    }
}

static void engine_fixed_reset(void *UNUSED(data))
{
    /* NOP */
}

static void engine_fixed_destroy(void *data)
{
    /*FETCH_DATA(data, upattern, UChar);

    free(upattern);*/
    FETCH_DATA(data, p, fixed_pattern_t);

    pattern_destroy(p);
}

#if 0
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
    // TODO: found a case insensitive way
    while (NULL != (m = u_strFindFirst(subject->ptr + pos, subject->len - pos, upattern, upattern_len))) {
        pos = m - subject->ptr;
        ustring_insert_len(subject, pos, before, before_len);
        ustring_insert_len(subject, pos + before_len + upattern_len, after, after_len);
        pos += before_len + upattern_len + after_len;
    }
}
#endif

engine_t fixed_engine = {
    engine_fixed_compile,
    engine_fixed_compileC,
    engine_fixed_pre_exec,
    engine_fixed_match,
    engine_fixed_whole_line_match,
    engine_fixed_reset,
    engine_fixed_destroy
};
