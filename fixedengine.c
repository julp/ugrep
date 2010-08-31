#include "ugrep.h"

#include <unicode/ubrk.h>

extern UBool wFlag; // for testing

typedef struct {
    UString *pattern;
    UBool case_insensitive;
} fixed_pattern_t;

static void pattern_destroy(fixed_pattern_t *p)
{
    ustring_destroy(p->pattern);
    free(p);
}

static void *engine_fixed_compile(const UChar *upattern, int32_t length, UBool case_insensitive)
{
    fixed_pattern_t *p;

    p = mem_new(*p);
    p->pattern = ustring_dup_string_len(upattern, length);
    p->case_insensitive = case_insensitive;

    return p;
}

static void *engine_fixed_compileC(const char *pattern, UBool case_insensitive)
{
    int32_t len;
    UConverter *ucnv;
    UErrorCode status;
    fixed_pattern_t *p;

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
    p->pattern->len = ucnv_toUChars(ucnv, p->pattern->ptr, p->pattern->allocated, pattern, len, &status);
    p->pattern->ptr[p->pattern->len] = U_NUL;
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        pattern_destroy(p);
        return NULL;
    }
    if (case_insensitive) {
        if (!ustring_tolower(p->pattern)) {
            pattern_destroy(p);
            return NULL;
        }
    }

    return p;
}

static void engine_fixed_pre_exec(void *data, UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    if (p->case_insensitive) {
        ustring_tolower(subject);
    }
}

static engine_return_t engine_fixed_match(void *data, const UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    // TODO: find better, inappropriate for binary file
    if (wFlag) {
        UChar *m;

        m = u_strFindFirst(subject->ptr, subject->len, p->pattern->ptr, p->pattern->len);
        if (NULL == m) {
            return ENGINE_NO_MATCH;
        } else {
#if 0
            UBool ret;
            UErrorCode status;
            UBreakIterator *bi;

            status = U_ZERO_ERROR;
            bi = ubrk_open(UBRK_WORD, "", &subject->ptr, subject->len, &status);
            if (U_FAILURE(status)) {
                icu(status, "ubrk_open");
                return ENGINE_FAILURE;
            }
            u_printf("start = %d ; end = %d ; string = %S\n", ubrk_isBoundary(bi, m - subject->ptr - 1), ubrk_isBoundary(bi, m - subject->ptr + p->pattern->len), subject->ptr);
            u_printf("start = %C ; end = %C\n", subject->ptr[m - subject->ptr], subject->ptr[m - subject->ptr + p->pattern->len])
            ret = ubrk_isBoundary(bi, m - subject->ptr - 1) && ubrk_isBoundary(bi, m - subject->ptr + p->pattern->len);
            ubrk_close(bi);

            return (ret ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
#endif
#define WORD_BOUNDARY(c) \
    (!u_isalnum(c) && 0x005f != c)

            return (
                ((m - subject->ptr == 0) || (WORD_BOUNDARY(subject->ptr[m - subject->ptr - 1])))
                &&
                ((m - subject->ptr + p->pattern->len == subject->len) || (WORD_BOUNDARY(subject->ptr[m - subject->ptr + p->pattern->len])))
            );
        }
    } else {
        return (NULL != u_strFindFirst(subject->ptr, subject->len, p->pattern->ptr, p->pattern->len) ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
    }
}

static engine_return_t engine_fixed_match_all(void *data, const UString *subject, slist_t *intervals)
{
    UChar *m;
    int32_t matches, pos;
    FETCH_DATA(data, p, fixed_pattern_t);

    matches = pos = 0;
    while (NULL != (m = u_strFindFirst(subject->ptr + pos, subject->len - pos, p->pattern->ptr, p->pattern->len))) {
        pos = m - subject->ptr;
        if (interval_add(intervals, subject->len, pos, pos + p->pattern->len)) {
            return ENGINE_WHOLE_LINE_MATCH;
        }
        pos += p->pattern->len;
    }

    return (matches ? ENGINE_MATCH_FOUND : ENGINE_NO_MATCH);
}

static engine_return_t engine_fixed_whole_line_match(void *data, const UString *subject)
{
    FETCH_DATA(data, p, fixed_pattern_t);

    if (p->case_insensitive) {
        return (0 == u_strcasecmp(p->pattern->ptr, subject->ptr, 0) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
    } else {
        return (0 == u_strcmp(p->pattern->ptr, subject->ptr) ? ENGINE_WHOLE_LINE_MATCH : ENGINE_NO_MATCH);
    }
}

static void engine_fixed_reset(void *UNUSED(data))
{
    /* NOP */
}

static void engine_fixed_destroy(void *data)
{
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
    engine_fixed_match_all,
    engine_fixed_whole_line_match,
    engine_fixed_reset,
    engine_fixed_destroy
};
