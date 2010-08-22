#include "ugrep.h"

#define USTRING_INITIAL_LENGTH 32 /*4096 */

UString *ustring_new()
{
    UString *ustr;

    ustr = mem_new(*ustr);
    ustr->len = 0;
    ustr->allocated = USTRING_INITIAL_LENGTH;
    ustr->ptr = mem_new_n(UChar, ustr->allocated + 1);
    *ustr->ptr = U_NUL;

    return ustr;
}

static void _ustring_maybe_expand(UString *ustr, size_t length)
{
    if (ustr->len + length >= ustr->allocated) {
        debug("Expand from %d to %d effective UChar", ustr->allocated, ustr->allocated*2);
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
    /*if (ustr->len >= ustr->allocated) {
        _ustring_expand(ustr);
    }
    ustr->ptr[ustr->len++] = c;
    ustr->ptr[ustr->len] = U_NUL;*/
    ustring_append_string_len(ustr, &c, 1);
}

void ustring_append_string(UString *ustr, const UChar *str)
{
    ustring_append_string_len(ustr, str, u_strlen(str));
}

/* WARNING: overlap are not managed ! */
void ustring_append_string_len(UString *ustr, const UChar *str, int32_t length)
{
    _ustring_maybe_expand(ustr, length);
    u_memcpy(ustr->ptr + ustr->len, str, length);
    ustr->len += length;
    ustr->ptr[ustr->len] = U_NUL;
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

UChar ustring_last_char(const UString *ustr)
{
    if (ustr->len) {
        return ustr->ptr[ustr->len - 1];
    } else {
        return U_NUL;
    }
}

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* !MIN */

void ustring_insert_len(UString *ustr, size_t position, UChar *c, size_t length)
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

    //return ustr;
}
