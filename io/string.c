#include <unistd.h>
#include <stdio.h>
#include <unicode/ustdio.h>
#include <errno.h>

#include "common.h"

typedef struct {
    size_t length;
    UConverter *ucnv;
    char *start, *end, *ptr;
} string_input_t;


static void *string_open(error_t **UNUSED(error), const char *buffer, int length) // "hack" for now
{
    string_input_t *this;

    this = mem_new(*this);
    this->ptr = this->start = buffer;
    this->length = length < 0 ? strlen(buffer) : (size_t) length;
    this->end = this->start + this->length;
    this->ucnv = NULL;

    return this;
}

static int32_t string_readuchars(error_t **error, void *data, UChar *buffer, size_t max_len)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_READUCHARS(error, this->ucnv, this->ptr, this->end, buffer, max_len);
}

static int32_t string_readuchars32(error_t **error, void *data, UChar32 *buffer, size_t max_len)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_READUCHARS32(error, this->ucnv, this->ptr, this->end, buffer, max_len);
}

static void string_rewind(void *data, int32_t signature_length)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_REWIND(this->start, this->ptr, signature_length);
}

static UBool string_readline(error_t **error, void *data, UString *ustr)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_READLINE(error, this->ucnv, this->ptr, this->end, ustr);
}

static size_t string_readbytes(void *data, char *buffer, size_t max_len)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_READBYTES(this->ptr, this->end, buffer, max_len);
}

static UBool string_has_encoding(void *data)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_HAS_ENCODING(this->ucnv);
}

static const char *string_get_encoding(error_t **error, void *data)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_GET_ENCODING(error, this->ucnv);
}

static UBool string_set_encoding(error_t **error, void *data, const char *encoding)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_SET_ENCODING(error, this->ucnv, encoding);
}

static UBool string_eof(void *data)
{
    FETCH_DATA(data, this, string_input_t);

    STRING_EOF(this->ptr, this->end);
}

static UBool string_seekable(void *UNUSED(data))
{
    STRING_SEEKABLE();
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
