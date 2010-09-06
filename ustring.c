#include <limits.h>

#include "ugrep.h"

#ifdef DEBUG
# define USTRING_INITIAL_LENGTH 8 /* Voluntarily small for development/test */
#else
# define USTRING_INITIAL_LENGTH 4096
#endif /* DEBUG */

#define SIZE_MAX_2 (SIZE_MAX << (sizeof(size_t) * CHAR_BIT - 1))

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

UString *ustring_new()
{
    return ustring_sized_new(USTRING_INITIAL_LENGTH);
}

UString *ustring_sized_new(size_t requested)
{
    UString *ustr;

    ustr = mem_new(*ustr);
    ustr->len = 0;
    ustr->allocated = nearest_power(requested);
    ustr->ptr = mem_new_n(UChar, ustr->allocated + 1);
    *ustr->ptr = U_NUL;

    return ustr;
}

static void _ustring_maybe_expand(UString *ustr, size_t length)
{
    if (ustr->len + length >= ustr->allocated) {
        //debug("Expand from %d to %d effective UChar", ustr->allocated, ustr->allocated*2);
        ustr->allocated *= 2;
        ustr->ptr = mem_renew(ustr->ptr, UChar, ustr->allocated + 1);
    }
}

void ustring_destroy(UString *ustr)
{
    free(ustr->ptr);
    free(ustr);
}

void ustring_append_char(UString *ustr, UChar c)
{
    ustring_insert_len(ustr, ustr->len, &c, 1);
}

void ustring_append_string(UString *ustr, const UChar *str)
{
    ustring_insert_len(ustr, ustr->len, str, u_strlen(str));
}

void ustring_append_string_len(UString *ustr, const UChar *str, int32_t len)
{
    ustring_insert_len(ustr, ustr->len, str, len);
}

void ustring_prepend_char(UString *ustr, UChar c)
{
    ustring_insert_len(ustr, 0, &c, 1);
}

void ustring_prepend_string(UString *ustr, const UChar *str)
{
    ustring_insert_len(ustr, 0, str, u_strlen(str));
}

void ustring_prepend_string_len(UString *ustr, const UChar *str, int32_t len)
{
    ustring_insert_len(ustr, 0, str, len);
}

UChar *ustring_chomp(UString *ustr)
{
    if (ustr->len > 0) {
        if (U_LF == ustr->ptr[ustr->len - 1]) {
            ustr->ptr[--ustr->len] = U_NUL;
            if (ustr->len > 0 && U_CR == ustr->ptr[ustr->len - 1]) {
                ustr->ptr[--ustr->len] = U_NUL;
            }
        }
    }

    return ustr->ptr;
}

void ustring_sync(const UString *ref, UString *buffer, double ratio)
{
    if (buffer->allocated <= ref->allocated * ratio) {
        buffer->allocated = ref->allocated * ratio;
        buffer->ptr = mem_renew(buffer->ptr, UChar, buffer->allocated + 1);
    }
}

UBool ustring_empty(const UString *ustr)
{
    return (ustr->len < 1);
}

void ustring_truncate(UString *ustr)
{
    *ustr->ptr = U_NUL;
    ustr->len = 0;
}

void ustring_insert_len(UString *ustr, size_t position, const UChar *c, size_t length)
{
    if (c >= ustr->ptr && c <= ustr->ptr + ustr->len) {
        size_t offset = c - ustr->ptr;
        size_t precount = 0;

        _ustring_maybe_expand(ustr, length);
        c = ustr->ptr + offset;

        if (position < ustr->len) {
            u_memmove(ustr->ptr + position + length, ustr->ptr + position, ustr->len - position);
        }
        if (offset < position) {
            precount = MIN(length, position - offset);
            u_memcpy(ustr->ptr + position, c, precount);
        }
        if (length > precount) {
            u_memcpy(ustr->ptr + position + precount, c + precount + length, length - precount);
        }
    } else {
        _ustring_maybe_expand(ustr, length);
        if (position < ustr->len) {
            u_memmove(ustr->ptr + position + length, ustr->ptr + position, ustr->len - position);
        }
        if (1 == length) {
            ustr->ptr[position] = *c;
        } else {
            u_memcpy(ustr->ptr + position, c, length);
        }
    }
    ustr->len += length;
    ustr->ptr[ustr->len] = U_NUL;
}

UString *ustring_dup_string_len(const UChar *from, int32_t length)
{
    UString *ustr;

    ustr = mem_new(*ustr);
    ustr->len = length;
    ustr->allocated = nearest_power(ustr->len);
    ustr->ptr = mem_new_n(UChar, ustr->allocated + 1);
    u_memcpy(ustr->ptr, from, ustr->len);
    ustr->ptr[ustr->len] = U_NUL;

    return ustr;
}

UString *ustring_dup_string(const UChar *from)
{
    return ustring_dup_string_len(from, u_strlen(from));
}

UString *ustring_adopt_string_len(UChar *from, int32_t len)
{
    UString *ustr;

    ustr = mem_new(*ustr);
    ustr->len = len;
    ustr->allocated = len + 1;
    ustr->ptr = from;

    return ustr;
}

UString *ustring_adopt_string(UChar *from)
{
    return ustring_adopt_string_len(from, u_strlen(from));
}

UBool ustring_tolower(UString *ustr, error_t **error)
{
    UErrorCode status;
    int32_t result_len;

    status = U_ZERO_ERROR;
    result_len = u_strFoldCase(ustr->ptr, ustr->len, ustr->ptr, ustr->len, 0, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "u_strFoldCase");
        return FALSE;
    }
    if (result_len != ustr->len) {
        error_set(error, FATAL, "Offsetted search is unreliable: the case-folded version has not the same length than the original");
        return FALSE;
    }

    return TRUE;
}
