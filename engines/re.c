#include "engine.h"

#include <unicode/ubrk.h>
#include <unicode/uregex.h>

static const UChar _UREGEXP_FAKE_USTR[] = { 0 };
#define UREGEXP_FAKE_USTR _UREGEXP_FAKE_USTR, 0

typedef struct {
    UBreakIterator *ubrk;
    URegularExpression *uregex;
} re_pattern_t;

static void re_pattern_reset(re_pattern_t *p)
{
    ubrk_unbindText(p->ubrk);
    uregex_unbindText(p->uregex);
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

static void *engine_re_compile(error_t **error, UString *ustr, uint32_t flags)
{
    re_pattern_t *p;
    UErrorCode status;
    UParseError pe = { -1, -1, {0}, {0} };

    status = U_ZERO_ERROR;
    p = mem_new(*p);
    p->ubrk = NULL;
    if (IS_WORD_BOUNDED(flags)) {
        UChar bsb[] = { 0x005c, 0x0062, 0 }; /* \b */

        if (!ustring_startswith(ustr, bsb, STR_LEN(bsb))) {
            ustring_prepend_string_len(ustr, bsb, STR_LEN(bsb));
        }
        if (!ustring_endswith(ustr, bsb, STR_LEN(bsb))) {
            ustring_append_string_len(ustr, bsb, STR_LEN(bsb));
        }
    }
    p->uregex = uregex_open(ustr->ptr, ustr->len, IS_CASE_INSENSITIVE(flags) ? UREGEX_CASE_INSENSITIVE : 0, &pe, &status);
    ustring_destroy(ustr); // ICU dups the pattern, so we can free it
    if (U_FAILURE(status)) {
        if (-1 != pe.line) {
            error_set(error, FATAL, "Invalid pattern: error at offset %d\n\t%S\n\t%*c\n", pe.offset, ustr->ptr, pe.offset, '^');
        } else {
            icu_error_set(error, FATAL, status, "uregex_open");
        }
        re_pattern_destroy(p);
        return NULL;
    }
//     if (IS_WORD_BOUNDED(flags)) {
//         p->ubrk = ubrk_open(UBRK_WORD, NULL, NULL, 0, &status);
//     } else {
        p->ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
//     }
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ubrk_open");
        re_pattern_destroy(p);
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

static engine_return_t engine_re_match_all(error_t **error, void *data, const UString *subject, interval_list_t *intervals)
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
            if (interval_list_add(intervals, subject->len, l, u)) {
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

static UBool uregex_fwd_n(
    URegularExpression *uregex,
    UBreakIterator *ubrk,
    const UString *subject,
    DArray *array, /* NULL to skip n matches */
    int32_t n,
    int32_t *last,
    error_t **error
) {
    int32_t l, u;
    UErrorCode status;

    status = U_ZERO_ERROR;
    while (n > 0 && U_SUCCESS(status) && uregex_findNext(uregex, &status)) {
        l = uregex_start(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_start");
            return FALSE;
        }
        u = uregex_end(uregex, 0, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "uregex_end");
            return FALSE;
        }
        if (ubrk_isBoundary(ubrk, l) && ubrk_isBoundary(ubrk, u)) {
            --n;
            if (NULL != array) {
                add_match(array, subject, *last, l);
            }
            *last = u;
        }
    }
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_findNext");
        return FALSE;
    }

    if (0 == n) {
        return TRUE;
    } else {
        if (NULL != array && *last < subject->len) {
            add_match(array, subject, *last, subject->len);
        }
        return FALSE;
    }
}

static UBool engine_re_split(error_t **error, void *data, const UString *subject, DArray *array, interval_list_t *intervals)
{
    UErrorCode status;
    dlist_element_t *el;
    int32_t l, lastU;
    FETCH_DATA(data, p, re_pattern_t);

    lastU = l = 0;
    status = U_ZERO_ERROR;
    uregex_setText(p->uregex, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "uregex_setText");
        return FALSE;
    }
    ubrk_setText(p->ubrk, subject->ptr, subject->len, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ubrk_setText");
        return FALSE;
    }
    for (el = intervals->head; NULL != el; el = el->next) {
        FETCH_DATA(el->data, i, interval_t);

        if (i->lower_limit > 0) {
            if (!uregex_fwd_n(p->uregex, p->ubrk, subject, NULL, i->lower_limit - lastU, &l, error)) {
                break;
            }
            l = uregex_end(p->uregex, 0, &status);
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "uregex_end");
                return FALSE;
            }
        }
        if (!uregex_fwd_n(p->uregex, p->ubrk, subject, array, i->upper_limit - i->lower_limit, &l, error)) {
            break;
        }
        lastU = i->upper_limit;
    }
    ubrk_unbindText(p->ubrk);
    if (NULL != *error) {
        return FALSE;
    }

    return TRUE;
}

static void engine_re_destroy(void *data)
{
    FETCH_DATA(data, p, re_pattern_t);

    re_pattern_destroy(p);
}

engine_t re_engine = {
    engine_re_compile,
    engine_re_match,
    engine_re_match_all,
    engine_re_whole_line_match,
    engine_re_split,
    engine_re_destroy
};
