#include <limits.h>
#include <ctype.h>

#include <unicode/ucol.h>
#include <unicode/uloc.h>

#include "common.h"

#ifdef DEBUG
# define USTRING_INITIAL_LENGTH 1 /* Voluntarily small for development/test */
#else
# define USTRING_INITIAL_LENGTH 4096
#endif /* DEBUG */

#define SIZE_MAX_2 (SIZE_MAX << (sizeof(size_t) * CHAR_BIT - 1))

#define USTRING_INIT_LEN(/*UString **/ ustr, /*size_t*/ length) \
    do {                                                        \
        ustr = mem_new(*ustr);                                  \
        ustr->ptr = NULL;                                       \
        ustr->allocated = ustr->len = 0;                        \
        _ustring_maybe_expand_to(ustr, (length));               \
    } while (0);

/* NOTE: /!\ all lengths are in code units not code point /!\ */

/* ==================== private helpers for growing up ==================== */

static inline size_t nearest_power(size_t requested_length)
{
    if (requested_length > SIZE_MAX_2) {
        return SIZE_MAX;
    } else {
        int i = 1;
        requested_length = MAX(requested_length, USTRING_INITIAL_LENGTH);
        while ((1UL << i) < requested_length) {
            i++;
        }

        return (1UL << i);
    }
}

static void _ustring_maybe_expand_of(UString *ustr, size_t additional_length) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    if (ustr->len + additional_length >= ustr->allocated) {
        //debug("Expand from %d to %d effective UChar", ustr->allocated, ustr->allocated * 2);
        ustr->allocated = nearest_power(ustr->len + additional_length);
        ustr->ptr = mem_renew(ustr->ptr, *ustr->ptr, ustr->allocated + 1);
    }
}

static void _ustring_maybe_expand_to(UString *ustr, size_t total_length) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    if (total_length >= ustr->allocated) {
        //debug("Expand from %d to %d effective UChar", ustr->allocated, ustr->allocated*2);
        ustr->allocated = nearest_power(total_length);
        ustr->ptr = mem_renew(ustr->ptr, *ustr->ptr, ustr->allocated + 1);
    }
}

/* ==================== creation and cloning ==================== */

UString *ustring_new(void) /* WARN_UNUSED_RESULT */
{
    return ustring_sized_new(USTRING_INITIAL_LENGTH);
}

UString *ustring_dup(const UString *ustr) /* WARN_UNUSED_RESULT NONNULL() */
{
    UString *copy;

    require_else_return_null(NULL != ustr);

    copy = mem_new(*copy);
    copy->allocated = copy->len = ustr->len;
    copy->ptr = mem_new_n(*copy->ptr, copy->allocated + 1);
    u_memcpy(copy->ptr, ustr->ptr, ustr->len);
    copy->ptr[copy->len] = 0;

    return copy;
}

UString *ustring_sized_new(size_t requested) /* WARN_UNUSED_RESULT */
{
    UString *ustr;

    USTRING_INIT_LEN(ustr, requested);
    *ustr->ptr = 0;

    return ustr;
}

UString *ustring_dup_string_len(const UChar *from, size_t length) /* NONNULL() */
{
    UString *ustr;

    require_else_return_null(NULL != from);

    USTRING_INIT_LEN(ustr, length);
    ustr->len = length;
    u_memcpy(ustr->ptr, from, ustr->len);
    ustr->ptr[ustr->len] = 0;

    return ustr;
}

UString *ustring_dup_string(const UChar *from) /* NONNULL() */
{
    require_else_return_null(NULL != from);

    return ustring_dup_string_len(from, (size_t) u_strlen(from));
}

UString *ustring_adopt_string_len(const UChar *from, size_t len)
{
    UString *ustr;

    ustr = mem_new(*ustr);
    ustr->len = len;
    ustr->allocated = len + 1;
    ustr->ptr = (UChar *) from;

    return ustr;
}

UString *ustring_adopt_string(const UChar *from) /* NONNULL() */
{
    require_else_return_null(NULL != from);

    return ustring_adopt_string_len(from, (size_t) u_strlen(from));
}

/* ==================== prefix/suffix ==================== */

UBool ustring_startswith(UString *ustr, UChar *str, size_t length) /* NONNULL() */
{
    return ustr->len >= length && 0 == u_memcmp(ustr->ptr, str, length);
}

UBool ustring_endswith(UString *ustr, UChar *str, size_t length) /* NONNULL() */
{
    return ustr->len >= length && 0 == u_memcmp(ustr->ptr + ustr->len - length, str, length);
}

/* ==================== handle argv ==================== */

#define ustring_delete_range_p(/*UString **/ ustr, /*UChar **/ from, /*UChar **/ to) \
    ustring_delete_len(ustr, from - ustr->ptr, to - from)

#define ustring_subreplace_len_p(/*UString **/ ustr, /*UChar **/ from, /*UChar **/ to, /*UChar **/ what, /*size_t*/ len) \
    ustring_subreplace_len(ustr, what, len, from - ustr->ptr, to - from)

static int hexadecimal_digit(UChar c)
{
    if (c >= U_0 && c <= U_9) { // '0' .. '9'
        return (c - U_0);
    }
    if (c >= U_A && c <= U_F) { // 'A' .. 'F'
        return (c - (U_A - 10));
    }
    if (c >= U_a && c <= U_f) { // 'a' .. 'f'
        return (c - (U_a - 10));
    }

    return -1;
}

void ustring_unescape(UString *ustr) /* NONNULL() */
{
    UChar lead;
    int digits, diff;
    const UChar *end;
    UBool trail_expected;
    UChar *p, *lead_offset;

    lead = 0;
    p = ustr->ptr;
    lead_offset = NULL;
    trail_expected = FALSE;
    end = ustr->ptr + ustr->len;
    while (p < end) {
        if (*p != '\\') {
            p++;
        } else {
            if ((end - p) < 1) { // assume \ is not the last character of the string
                break;
            }
            switch (p[1]) {
                case U_u: // u
                    digits = 4;
                    break;
                case U_U: // U
                    digits = 8;
                    break;
#if 0
                //case 0x61: // a, BEL, 0x07
                case 0x62: // b
                case 0x65: // e
                case 0x66: // f
                case 0x6E: // n
                case 0x72: // r
                case 0x74: // t
                case 0x76: // v
                    if (TRUE) {
                        static const UChar map[] = {
                            0, U_BS, 0, 0, 0x1B, U_FF, 0, 0, 0, 0, 0, 0, 0, U_LF, 0, 0, 0, U_CR, 0, U_HT, 0, U_VT, 0, 0, 0, 0
                        };

                        digits = 0;
//                         if (0 != map[p[1] - U_a]) {
                            ustring_subreplace_len_p(ustr, p, p + 1, &map[p[1] - U_a], 1);
//                         }
                        break;
                    }
#endif
                default:
                    digits = 0;
                    p += STR_LEN("\\X"); // p[0] is \ ; p[1] have been read
            }
            if (digits > 0) {
                int n;
                UChar *s;
                UChar32 result;

                s = p + STR_LEN("\\X");
                n = 0;
                result = 0;
                while (s < end && n < digits) {
                    int digit;

                    digit = hexadecimal_digit(*s);
                    if (digit < 0) {
                        break;
                    }
                    result = (result << 4) | digit;
                    ++n;
                    ++s;
                }
                if (n != digits) {
                    goto failed;
                }
                if (result < 0 || result > UCHAR_MAX_VALUE || U_IS_UNICODE_NONCHAR(result)) {
                    goto failed;
                }
                if (4 == digits) { // \uXXXX
                    if (!U_IS_SURROGATE(result)) {
                        if (trail_expected) {
                            goto failed;
                        } else {
                            diff = ustring_subreplace_len_p(ustr, p, s, (UChar *) &result, 1);
                            assert(diff == -5); // -(STR_LEN("\\uXXXX") - 1 UChar)
                            p += diff;
                            end += diff;
                        }
                    } else if (U16_IS_SURROGATE_LEAD(result)) {
                        trail_expected = TRUE;
                        lead = result;
                        lead_offset = p;
                        //p += STR_LEN("\\uXXXX");
                        p = s;
                    } else if (U16_IS_SURROGATE_TRAIL(result)) {
                        if (trail_expected && ((2 * STR_LEN("\\uXXXX")) == (s - lead_offset))) {
                            UChar trail = result;
                            trail_expected = FALSE;
                            result = U16_GET_SUPPLEMENTARY(lead, result);
                            if (!U_IS_UNICODE_CHAR(result)) {
                                p = lead_offset; // we need to delete both CU
                                goto failed;
                            } else {
                                UChar tmp[] = { lead, trail };
                                diff = ustring_subreplace_len_p(ustr, lead_offset, s, tmp, 2);
                                assert(diff == -10); // -(STR_LEN("\\uXXXX\\uXXXX") - 2 UChar)
                                p += diff;
                                end += diff;
                            }
                        } else {
                            goto failed;
                        }
                    }
                } else if (8 == digits) { // \UXXXXXXXX
                    if (!U_IS_UNICODE_CHAR(result)) {
                        goto failed;
                    } else {
                        int32_t len;
                        UChar tmp[U16_MAX_LENGTH];

                        len = 0;
                        U16_APPEND_UNSAFE(tmp, len, result);
                        diff = ustring_subreplace_len_p(ustr, p, s, tmp, len);
                        assert(diff == -((int) (STR_LEN("\\Uxxxxxxxx") - U16_LENGTH(result))));
                        p += diff;
                        end += diff;
                    }
                }
                if (FALSE) {
failed:
                    diff = ustring_delete_range_p(ustr, p, s);
                    p += diff;
                    end += diff;
                }
            }
        }
    }
    if (trail_expected) {
        ustring_delete_len(ustr, lead_offset - ustr->ptr, STR_LEN("\\uXXXX")); // we already have assumed that sequence length is correct
    }
}

UString *ustring_convert_argv_from_local(const char *cargv, error_t **error, UBool unescape)
{
    UString *ustr;
    UConverter *ucnv;
    UErrorCode status;
    int32_t cargv_length;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(env_get_stdin_encoding(), &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
        return NULL;
    }
    cargv_length = strlen(cargv);
    USTRING_INIT_LEN(ustr, cargv_length * ucnv_getMaxCharSize(ucnv));
    ustr->len = ucnv_toUChars(ucnv, ustr->ptr, ustr->allocated, cargv, cargv_length, &status);
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        ustring_destroy(ustr);
        icu_error_set(error, FATAL, status, "ucnv_toUChars");
        return NULL;
    }
    ustr->ptr[ustr->len] = 0;
    if (unescape) {
        ustring_unescape(ustr);
    }
    ustring_normalize(ustr, env_get_normalization());

    return ustr;
}

/* ==================== destruction ==================== */

void ustring_destroy(UString *ustr) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    free(ustr->ptr);
    free(ustr);
}

UChar *ustring_orphan(UString *ustr) /* NONNULL() */
{
    UChar *ret;

    ret = ustr->ptr;
    free(ustr);

    return ret;
}

/* ==================== basic operations (insert, append, replace, delete) ==================== */

int32_t ustring_subreplace_len(UString *ustr, const UChar *replacement, size_t replacement_length, size_t position, size_t length) /* NONNULL(1) */
{
    int32_t diff_len;

    require_else_return_zero(NULL != ustr);
    require_else_return_zero(position <= ustr->len);

    diff_len = replacement_length - length;
    if (diff_len > 0) {
        _ustring_maybe_expand_of(ustr, diff_len);
    }
    if (replacement_length != length) {
        // TODO: assume ustr->len - position - length > 0?
        u_memmove(ustr->ptr + position + length + diff_len, ustr->ptr + position + length, ustr->len - position - length);
    }
    if (replacement_length > 0) {
        if (replacement >= ustr->ptr && replacement <= ustr->ptr + ustr->len) {
            size_t offset = replacement - ustr->ptr;
            size_t precount = 0;

            replacement = ustr->ptr + offset;
            if (offset < position) {
                precount = MIN(replacement_length, position - offset);
                u_memcpy(ustr->ptr + position, replacement, precount);
            }
            if (length > precount) {
                u_memcpy(ustr->ptr + position + precount, replacement + precount + replacement_length, replacement_length - precount);
            }
        } else {
            u_memcpy(ustr->ptr + position, replacement, replacement_length);
        }
    }
    ustr->len += diff_len;
    ustr->ptr[ustr->len] = 0;

    return diff_len;
}

int32_t ustring_delete_len(UString *ustr, size_t position, size_t length) /* NONNULL(1) */
{
    return ustring_subreplace_len(ustr, NULL, 0, position, length);
}

int32_t ustring_insert_len(UString *ustr, size_t position, const UChar *c, size_t length) /* NONNULL(1) */
{
    return ustring_subreplace_len(ustr, c, length, position, 0);
}

void ustring_append_char32(UString *ustr, UChar32 c) /* NONNULL() */
{
    _ustring_maybe_expand_of(ustr, 2);

    U16_APPEND_UNSAFE(ustr->ptr, ustr->len, c);
    ustr->ptr[ustr->len] = 0;
}

void ustring_append_char(UString *ustr, UChar c) /* NONNULL() */
{
    ustring_insert_len(ustr, ustr->len, &c, 1);
}

void ustring_append_string(UString *ustr, const UChar *str) /* NONNULL() */
{
    ustring_insert_len(ustr, ustr->len, str, u_strlen(str));
}

void ustring_append_string_len(UString *ustr, const UChar *str, int32_t len) /* NONNULL() */
{
    ustring_insert_len(ustr, ustr->len, str, len);
}

void ustring_prepend_char(UString *ustr, UChar c) /* NONNULL() */
{
    ustring_insert_len(ustr, 0, &c, 1);
}

void ustring_prepend_string(UString *ustr, const UChar *str) /* NONNULL() */
{
    ustring_insert_len(ustr, 0, str, u_strlen(str));
}

void ustring_prepend_string_len(UString *ustr, const UChar *str, int32_t len) /* NONNULL() */
{
    ustring_insert_len(ustr, 0, str, len);
}

/* ==================== trimming (WS and/or EOL) ==================== */

void ustring_chomp(UString *ustr) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    if (ustr->len > 0) {
        switch (ustr->ptr[ustr->len - 1]) {
            case U_CR:
            case U_VT:
            case U_FF:
            case U_NL:
            case U_LS:
            case U_PS:
                ustr->ptr[--ustr->len] = 0;
                break;
            case U_LF:
                ustr->ptr[--ustr->len] = 0;
                if (ustr->len > 0 && U_CR == ustr->ptr[ustr->len - 1]) {
                    ustr->ptr[--ustr->len] = 0;
                }
                break;
        }
    }
}

enum {
    TRIM_LEFT  = 1,
    TRIM_RIGHT = 2,
    TRIM_BOTH  = 3
};

static int32_t _u_trim(
    UChar *string, int32_t string_length,
    UChar *what, int32_t what_length,
    int mode
) {
    int32_t i, k;
    UChar32 c = 0;
    int32_t start = 0, end;
    int32_t string_cu_length, what_cu_length;

    what_cu_length = 0;
    if (string_length < 0) {
        string_cu_length = u_strlen(string);
    } else {
        string_cu_length = string_length;
    }
    if (NULL != what) {
        if (0 == *what) {
            what = NULL;
        } else if (what_length < 0) {
            what_cu_length = u_strlen(what);
        } else {
            what_cu_length = what_length;
        }
    }
    end = string_cu_length;
    if (mode & TRIM_LEFT) {
        for (i = k = 0 ; i < end ; ) {
            U16_NEXT(string, k, end, c);
            if (NULL != what) {
                if (NULL == u_memchr32(what, c, what_cu_length)) {
                    break;
                }
            } else {
                if (FALSE == u_isWhitespace(c)) {
                    break;
                }
            }
            i = k;
        }
        start = i;
    }
    if (mode & TRIM_RIGHT) {
        for (i = k = end ; i > start ; ) {
            U16_PREV(string, 0, k, c);
            if (NULL != what) {
                if (NULL == u_memchr32(what, c, what_cu_length)) {
                    break;
                }
            } else {
                if (FALSE == u_isWhitespace(c)) {
                    break;
                }
            }
            i = k;
        }
        end = i;
    }
    if (start < string_cu_length) {
        u_memmove(string, string + start, end - start);
        *(string + end - start) = 0;
    } else {
        *string = 0;
    }

    return end - start;
}

static int32_t u_trim(UChar *s, int32_t s_length, UChar *what, int32_t what_length)
{
    return _u_trim(s, s_length, what, what_length, TRIM_BOTH);
}

static int32_t u_ltrim(UChar *s, int32_t s_length, UChar *what, int32_t what_length)
{
    return _u_trim(s, s_length, what, what_length, TRIM_LEFT);
}

static int32_t u_rtrim(UChar *s, int32_t s_length, UChar *what, int32_t what_length)
{
    return _u_trim(s, s_length, what, what_length, TRIM_RIGHT);
}

void ustring_trim(UString *ustr) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    ustr->len = u_trim(ustr->ptr, ustr->len, NULL, -1);
}

void ustring_ltrim(UString *ustr) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    ustr->len = u_ltrim(ustr->ptr, ustr->len, NULL, -1);
}

void ustring_rtrim(UString *ustr) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    ustr->len = u_rtrim(ustr->ptr, ustr->len, NULL, -1);
}

/* ==================== full case mapping ==================== */

static UBool uloc_is_turkic(const char *locale)
{
    if (NULL == locale) {
        locale = uloc_getDefault();
    }
    if (NULL != locale) {
        if (strlen(locale) >= 2) {
            if (
                ('a' == tolower(locale[0]) && 'z' == tolower(locale[1]) && ('_' == locale[2] || '\0' == locale[2]))
                ||
                ('t' == tolower(locale[0]) && 'r' == tolower(locale[1]) && ('_' == locale[2] || '\0' == locale[2]))
            ) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

typedef int32_t (*func_full_case_mapping_t)(UChar *, int32_t, const UChar *, int32_t, const char *, UErrorCode *);

static int32_t u_strFoldCaseEx(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, const char *locale, UErrorCode *status)
{
    return u_strFoldCase(dest, destCapacity, src, srcLength, uloc_is_turkic(locale) ? U_FOLD_CASE_EXCLUDE_SPECIAL_I : U_FOLD_CASE_DEFAULT, status);
}

static int32_t u_strToTitleEx(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, const char *locale, UErrorCode *status)
{
    return u_strToTitle(dest, destCapacity, src, srcLength, NULL, locale, status);
}

struct case_mapping_t {
    const char *name; // ICU real/called name function
    func_full_case_mapping_t func;
};

static const struct case_mapping_t unicode_case_mapping[UCASE_COUNT] = {
    { NULL,            NULL },
    { "u_strFoldCase", u_strFoldCaseEx },
    { "u_strToLower",  u_strToLower },
    { "u_strToUpper",  u_strToUpper },
    { "u_strToTitle",  u_strToTitleEx }
};

UBool ustring_fullcase(UString *ustr, UChar *src, int32_t src_len, UCaseType ct, error_t **error) /* NONNULL(1) */
{
    int32_t len;
    UErrorCode status;

    require_else_return_false(NULL != ustr);
    require_else_return_false(ct >= UCASE_NONE && ct < UCASE_COUNT);

    status = U_ZERO_ERROR;
    if (UCASE_NONE == ct) {
        _ustring_maybe_expand_to(ustr, ustr->len = src_len);
        // TODO: manage overlap?
        u_memcpy(ustr->ptr, src, src_len);
        ustr->ptr[ustr->len] = 0;
        return TRUE;
    }
    len = unicode_case_mapping[ct].func(NULL, 0, src, src_len, NULL, &status);
    if (U_BUFFER_OVERFLOW_ERROR != status && U_STRING_NOT_TERMINATED_WARNING != status) {
        return FALSE;
    }
    status = U_ZERO_ERROR;
    _ustring_maybe_expand_to(ustr, len);
    ustr->len = unicode_case_mapping[ct].func(ustr->ptr, ustr->allocated + 1, src, src_len, NULL, &status);
    if (U_FAILURE(status)) {
        error_set(error, FATAL, "ICU Error \"%s\" from %s()", u_errorName(status), unicode_case_mapping[ct].name);
        return FALSE;
    } else {
        ustr->ptr[ustr->len] = 0;
        return TRUE;
    }
}

/* ==================== misc/others ==================== */

#if 0
enum {
    LT, // <
    LE, // <=
    EQ, // ==
    NE, // !=
    GE, // >=
    GT  // >
};

UBool ustring_char32_len_cmp(const UString *ustr, int operator, int32_t length)
{
    switch (operator) {
        case LT:
            --length;
        case LE:
            return !u_strHasMoreChar32Than(ustr->ptr, ustr->len, length);
        case EQ:
            return u_countChar32(ustr->ptr, ustr->len) == length;
        case NE:
            return u_countChar32(ustr->ptr, ustr->len) != length;
        case GE:
            ++length;
        case GT:
            return u_strHasMoreChar32Than(ustr->ptr, ustr->len, length);
    }

    return FALSE;
}
#endif

void ustring_sync(const UString *ref, UString *buffer, double ratio) /* NONNULL() */
{
    require_else_return(NULL != ref);
    require_else_return(buffer != ref);
    require_else_return(0 != ratio);

    if (buffer->allocated <= ref->allocated * ratio) {
        buffer->allocated = ref->allocated * ratio;
        buffer->ptr = mem_renew(buffer->ptr, *buffer->ptr, buffer->allocated + 1);
    }
}

UBool ustring_empty(const UString *ustr) /* NONNULL() */
{
    require_else_return_false(NULL != ustr);

    return (ustr->len < 1);
}

void ustring_truncate(UString *ustr) /* NONNULL() */
{
    require_else_return(NULL != ustr);

    *ustr->ptr = 0;
    ustr->len = 0;
}

void ustring_dump(UString *ustr) /* NONNULL() */
{
    UChar *p;
    UChar32 c;
    size_t i, len;
    const int replacement_len = 6;
    const char replacement[] = "0x%04X";

    require_else_return(NULL != ustr);

    len = 0;
    for (i = 0; i < ustr->len; ) {
        U16_NEXT(ustr->ptr, i, ustr->len, c);
        switch (c) {
            case U_HT:
            case U_CR:
                len++;
                break;
            default:
                if (!u_isprint(c)) {
                    len += replacement_len - U16_LENGTH(c);
                }
        }
    }
    if (len > 0) {
        _ustring_maybe_expand_of(ustr, len);
        ustr->len += len;
        ustr->ptr[ustr->len] = 0;
        p = ustr->ptr + ustr->len;
        while (i > 0) {
            U16_PREV(ustr->ptr, 0, i, c);
            switch (c) {
                case U_HT:
                    *--p = 0x74;
                    *--p = 0x5C;
                    break;
                case U_CR:
                    *--p = 0x72;
                    *--p = 0x5C;
                    break;
                default:
                    if (!u_isprint(c)) {
                        p -= replacement_len;
                        u_snprintf(p, replacement_len, replacement, c);
                    } else {
                        if (U_IS_BMP(c)) {
                            *--p = c;
                        } else {
                            *--p = U16_LEAD(c);
                            *--p = U16_TRAIL(c);
                        }
                    }
            }
        }
    }
}

void ustring_sprintf(UString *ustr, const char *format, ...) /* NONNULL(1, 2) */
{
    va_list args;
    int32_t ret;

    require_else_return(NULL != ustr);
    require_else_return(NULL != format);

    va_start(args, format);
    if ((ret = u_vsnprintf(ustr->ptr, ustr->allocated, format, args)) > (int32_t) ustr->allocated) {
        do {
            ustr->allocated *= 2;
            ustr->ptr = mem_renew(ustr->ptr, *ustr->ptr, ustr->allocated + 1);
            va_start(args, format);
            ret = u_vsnprintf(ustr->ptr, ustr->allocated, format, args);
            va_end(args);
        } while (ret > (int32_t) ustr->allocated);
    } else {
        va_end(args);
    }
    ustr->len = ret;
}

/* ==================== normalization ==================== */

static UChar *u_strdup(UChar *src, int32_t len)
{
    UChar *cpy;

    if (len < 0) {
        len = u_strlen(src);
    }
    cpy = mem_new_n(*cpy, len + 1);
    u_memcpy(cpy, src, len);
    cpy[len] = 0;

    return cpy;
}

UBool ustring_normalize(UString *ustr, UNormalizationMode mode)
{
    UErrorCode status;

    require_else_return_false(NULL != ustr);

    status = U_ZERO_ERROR;
    if (UNORM_NONE != mode && ustr->len > 0) {
        UChar *tmp;
        int32_t res_len;

        tmp = u_strdup(ustr->ptr, ustr->len);
        res_len = unorm_normalize(tmp, ustr->len, mode, 0, NULL, 0, &status);
        if (U_BUFFER_OVERFLOW_ERROR == status) {
            status = U_ZERO_ERROR;
            _ustring_maybe_expand_to(ustr, res_len);
            ustr->len = unorm_normalize(tmp, ustr->len, mode, 0, ustr->ptr, ustr->allocated, &status);
        }
        free(tmp);
    }

    return U_SUCCESS(status);
}

/* ==================== collation ==================== */

void *ustring_to_collation_key(const void *value, const void *data) /* NONNULL() */
{
    uint8_t *key;
    int32_t key_len;
    const UString *ustr;
    const UCollator *ucol;

    ustr = (const UString *) value;
    ucol = (const UCollator *) data;
    key_len = ucol_getSortKey(ucol, ustr->ptr, ustr->len, NULL, 0);
    key = mem_new_n(*key, key_len + 1);
    ensure(key_len == ucol_getSortKey(ucol, ustr->ptr, ustr->len, key, key_len));
    key[key_len] = 0;

    return key;
}
