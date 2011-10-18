#include "common.h"

typedef struct {
    size_t length;
    const char *start, *end, *ptr;
} STRING;

void *string_open(const char *buffer, int length)
{
    STRING *this;

    this = mem_new(*this);
    this->ptr = this->start = buffer;
    this->length = length < 0 ? strlen(buffer) : (size_t) length;
    this->end = this->start + this->length;

    return this;
}

static void string_close(void *fp)
{
    free(fp);
}

static UBool string_eof(void *fp)
{
    STRING *this;

    this = (STRING *) fp;

    return this->ptr >= this->end;
}

static UBool string_rewindTo(void *fp, error_t **UNUSED(error), int32_t signature_length)
{
    STRING *this;

    this = (STRING *) fp;
    this->ptr = this->start + signature_length;

    return TRUE;
}

static int32_t string_readBytes(void *fp, error_t **UNUSED(error), char *buffer, size_t max_len)
{
    int n;
    STRING *this;

    this = (STRING *) fp;
    if ((size_t) (this->end - this->ptr) > max_len) {
        n = max_len;
    } else {
        n = this->end - this->ptr;
    }
    memcpy(buffer, this->ptr, n);
    this->ptr += n;

    return n;
}

reader_imp_t string_reader_imp =
{
    TRUE,
    "string",
    NULL,
    string_close,
    string_eof,
    string_readBytes,
    string_rewindTo
};
