#include "ugrep.h"

#define USTRING_INITIAL_LENGTH 32 /*4096 */

UString *ustring_new()
{
    UString *ustr;

    ustr = mem_new(*ustr);
    ustr->len = 0;
    ustr->allocated = USTRING_INITIAL_LENGTH;
    ustr->ptr = mem_new_n(UChar, ustr->allocated + 1);
    *ustr->ptr = U_EOB;

    return ustr;
}

static void _ustring_expand(UString *ustr)
{
    debug("Expand from %d to %d effective UChar", ustr->allocated, ustr->allocated*2);
    ustr->allocated *= 2;
    ustr->ptr = mem_renew(ustr->ptr, UChar, ustr->allocated + 1);
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
    ustr->ptr[ustr->len] = U_EOB;*/
    ustring_append_string_len(ustr, &c, 1);
}

void ustring_append_string(UString *ustr, const UChar *str)
{
    ustring_append_string_len(ustr, str, u_strlen(str));
}

/* WARNING: overlap are not managed ! */
void ustring_append_string_len(UString *ustr, const UChar *str, int32_t length)
{
    if (ustr->len + length >= ustr->allocated) {
        _ustring_expand(ustr);
    }
    u_memcpy(ustr->ptr + ustr->len, str, length);
    ustr->len += length;
    ustr->ptr[ustr->len] = U_EOB;
}

UChar *ustring_chomp(UString *ustr)
{
    if (ustr->len > 0) {
        if (U_LF == ustr->ptr[ustr->len - 1]) {
            ustr->ptr[ustr->len--] = U_EOB;
            if (ustr->len > 0 && U_CR == ustr->ptr[ustr->len - 1]) {
                ustr->ptr[ustr->len--] = U_EOB;
            }
        }
    }

    return ustr->ptr;
}

void ustring_sync(const UString *ustr, UString *buffer, double ratio)
{
    if (buffer->allocated <= ustr->allocated * ratio) {
        buffer->allocated = ustr->allocated * ratio;
        buffer->ptr = mem_renew(buffer->ptr, UChar, buffer->allocated + 1);
    }
}

UBool ustring_empty(const UString *ustr)
{
    return (ustr->len < 1);
}

void ustring_truncate(UString *ustr)
{
    *ustr->ptr = U_EOB;
    ustr->len = 0;
}

UChar ustring_last_char(const UString *ustr)
{
    if (ustr->len) {
        return ustr->ptr[ustr->len - 1];
    } else {
        return U_EOB;
    }
}
