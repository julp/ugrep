#include "ugrep.h"

#include <unistd.h>
#include <stdio.h>
#include <unicode/ustdio.h>

#include <errno.h>

typedef struct {
    size_t length;
    UConverter *ucnv;
    char *start, *end, *ptr;
} string_input_t;

static void *string_open(error_t **error, const char *buffer, int length)
{
    string_input_t *this;

    this = mem_new(*this);
    this->ptr = this->start = buffer;
    this->length = length < 0 ? strlen(buffer) : length;
    this->end = this->start + this->length;
    this->ucnv = NULL;

    return this;
}

static int32_t string_readuchars(error_t **error, void *data, UChar *buffer, size_t max_len)
{
    int32_t count;
    UErrorCode status;
    FETCH_DATA(data, this, string_input_t);

    status = U_ZERO_ERROR;
    count = ucnv_toUChars(this->ucnv, buffer, max_len, this->ptr, MIN(this->end - this->ptr, max_len), &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_toUChars");
        count = -1;
    } else {
        this->ptr += count;
    }

    return count;
}

static int32_t string_readuchars32(error_t **error, void *data, UChar32 *buffer, size_t max_len)
{
    UChar32 c;
    int32_t i, len;
    UErrorCode status;
    FETCH_DATA(data, this, string_input_t);

    status = U_ZERO_ERROR;
    len = this->length > max_len ? max_len : this->length;
    for (i = 0; i < len; i++) {
        c = ucnv_getNextUChar(this->ucnv, (const char **) &this->ptr, this->end, &status);
        if (U_FAILURE(status)) {
            if (U_INDEX_OUTOFBOUNDS_ERROR == status) {
                break;
            } else {
                icu_error_set(error, FATAL, status, "ucnv_getNextUChar");
                return -1;
            }
        }
        buffer[i] = c;
    }

    return i;
}

static void string_rewind(void *data, int32_t signature_length)
{
    FETCH_DATA(data, this, string_input_t);

    this->ptr = this->start + signature_length;
}

static UBool string_readline(error_t **error, void *data, UString *ustr)
{
    UChar32 c;
    UErrorCode status;
    FETCH_DATA(data, this, string_input_t);

    status = U_ZERO_ERROR;
    do {
        c = ucnv_getNextUChar(this->ucnv, (const char **) &this->ptr, this->end, &status);
        if (U_FAILURE(status)) {
            if (U_INDEX_OUTOFBOUNDS_ERROR == status) { // c == U_EOF
                /*if (!ustring_empty(ustr)) {
                    break;
                } else {
                    return FALSE;
                }*/
                break;
            } else {
                icu_error_set(error, FATAL, status, "ucnv_getNextUChar");
                return FALSE;
            }
        }
        ustring_append_char(ustr, c);
    } while (U_LF != c);

    return TRUE;
}

static size_t string_readbytes(void *data, char *buffer, size_t max_len)
{
    size_t n;
    FETCH_DATA(data, this, string_input_t);

    if (this->end - this->ptr > max_len) {
        n = max_len;
    } else {
        n = this->end - this->ptr;
    }
    memcpy(buffer, this->ptr, n);
    this->ptr += n;

    return n;
}

static UBool string_has_encoding(void *data)
{
    FETCH_DATA(data, this, string_input_t);

    return NULL != this->ucnv;
}

static const char *string_get_encoding(void *data)
{
    UErrorCode status;
    const char *encoding;
    FETCH_DATA(data, this, string_input_t);

    status = U_ZERO_ERROR;
    encoding = ucnv_getName(this->ucnv, &status);
    if (U_SUCCESS(status)) {
        return encoding;
    } else {
        //icu_error_set(error, FATAL, status, "ucnv_getName");
        return "ucnv_getName() failed";
    }
}

static UBool string_set_encoding(error_t **error, void *data, const char *encoding)
{
    UErrorCode status;
    FETCH_DATA(data, this, string_input_t);

    status = U_ZERO_ERROR;
    this->ucnv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
    }

    return U_SUCCESS(status);
}

static UBool string_eof(void *data)
{
    FETCH_DATA(data, this, string_input_t);

    return this->ptr >= this->end;
}

static UBool string_seekable(void *UNUSED(data))
{
    return TRUE;
}

reader_imp_t string_reader_imp =
{
    TRUE,
    "string",
    string_open,
    NULL,
    string_eof,
    string_seekable,
    string_readline,
    string_readbytes,
    string_readuchars,
    string_readuchars32,
    string_has_encoding,
    string_get_encoding,
    string_set_encoding,
    string_rewind
};
