#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <unicode/ucsdet.h>

#include "common.h"

/* ==================== global ==================== */

extern reader_imp_t mmap_reader_imp;
extern reader_imp_t stdio_reader_imp;
extern reader_imp_t string_reader_imp;
#if defined(HAVE_ZLIB) || defined(DYNAMIC_READERS)
extern reader_imp_t zlib_reader_imp;
#endif /* HAVE_ZLIB || DYNAMIC_READERS */
#if defined(HAVE_BZIP2) || defined(DYNAMIC_READERS)
extern reader_imp_t bzip2_reader_imp;
#endif /* HAVE_BZIP2 || DYNAMIC_READERS */
#if defined(HAVE_LZMA) || defined(DYNAMIC_READERS)
extern reader_imp_t lzma_reader_imp;
#endif /* HAVE_LZMA || DYNAMIC_READERS */

static const reader_imp_t *available_readers[] = {
    &mmap_reader_imp,
    &stdio_reader_imp,
#if defined(HAVE_ZLIB) || defined(DYNAMIC_READERS)
    &zlib_reader_imp,
#endif /* HAVE_ZLIB || DYNAMIC_READERS */
#if defined(HAVE_BZIP2) || defined(DYNAMIC_READERS)
    &bzip2_reader_imp,
#endif /* HAVE_BZIP2 || DYNAMIC_READERS */
#if defined(HAVE_LZMA) || defined(DYNAMIC_READERS)
    &lzma_reader_imp,
#endif /* HAVE_LZMA || DYNAMIC_READERS */
    NULL
};

void *string_open(const char *buffer, int length);

/* ==================== private helpers for reading ==================== */

static int32_t fill_buffer(reader_t *this, error_t **error)
{
    size_t utf16diff, bytesdiff;
    UChar *utf16Ptr;
    UErrorCode status;
    int32_t bytesAvailable, bytesRead, utf16Length, maxBytesToRead;

    utf16Length = 0;
    status = U_ZERO_ERROR;
    utf16diff = this->utf16.ptr - this->utf16.buffer;
    if (utf16diff > 0) {
        u_memmove(this->utf16.buffer, this->utf16.ptr, this->utf16.end - this->utf16.ptr);
        this->utf16.end -= utf16diff;
        this->utf16.ptr = this->utf16.buffer;
    }
    bytesdiff = this->byte.ptr - this->byte.buffer;
    if (bytesdiff > 0 && this->byte.end > this->byte.ptr) {
        memmove(this->byte.buffer, this->byte.ptr, this->byte.end - this->byte.ptr);
        this->byte.end -= bytesdiff;
        this->byte.ptr = this->byte.buffer;
    } else {
        this->byte.end = this->byte.ptr = this->byte.buffer;
    }
    bytesAvailable = this->byte.limit - this->byte.end;
    maxBytesToRead = MIN(bytesAvailable / (2 * ucnv_getMinCharSize(this->ucnv)), CHAR_BUFFER_SIZE);
    if (-1 == (bytesRead = this->imp->readBytes(this->fp, error, this->byte.ptr, maxBytesToRead))) {
        return -1;
    }
    this->byte.end = this->byte.ptr + bytesRead;
    utf16Ptr = this->utf16.ptr;
    ucnv_toUnicode(
        this->ucnv,
        &utf16Ptr, this->utf16.limit,
        (const char **) &this->byte.ptr, this->byte.end,
        NULL,
        this->imp->eof(this->fp),
        &status
    );
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_toUnicode");
        return -1;
    }
    utf16Length = utf16Ptr - this->utf16.ptr;
    this->utf16.end = utf16Ptr;

    return utf16Length;
}

/* ==================== public functions for reading ==================== */

static void copy_full_buffer_into_ustring(reader_t *this, UString *ustr)
{
    ustring_append_string_len(ustr, this->utf16.ptr, this->utf16.end - this->utf16.ptr);
    this->utf16.ptr = this->utf16.end;
}

static void copy_buffer_until_into_ustring(reader_t *this, UString *ustr, UChar *includedLimit)
{
    ustring_append_string_len(ustr, this->utf16.ptr, includedLimit - this->utf16.ptr + 1);
    this->utf16.ptr = ++includedLimit;
}

/**
 * Return:
 * - TRUE: read can continue (no error, no EOF)
 * - FALSE: read should be stopped (error or EOF - check error == NULL if passed)
 **/
UBool reader_readline(reader_t *this, error_t **error, UString *ustr) /* NONNULL(1, 3) */
{
    UChar *p;
    int32_t available;

    require_else_return_false(NULL != this);
    require_else_return_false(NULL != ustr);

    ustring_truncate(ustr);
    available = this->utf16.end - this->utf16.ptr;
    if (0 == available) {
        //if (-1 == (available = fill_buffer(this, error))) {
        if ((available = fill_buffer(this, error)) < 1) {
            return FALSE;
        }
    }
    while (available > 0) {
        p = this->utf16.ptr;
        while (p < this->utf16.end) {
            switch (*p) {
                case U_CR:
                    if ((p + 1) >= this->utf16.end) {
                        copy_full_buffer_into_ustring(this, ustr);
                        if (-1 == fill_buffer(this, error)) {
                            return FALSE;
                        }
                        p = this->utf16.ptr;
                        if (U_LF == *p) {
                            ustring_append_char(ustr, *p);
                            ++this->utf16.ptr;
                        }
                        goto eol;
                    }
                    if (U_LF == *(p + 1)) {
                        ++p;
                    }
                    /* no break here */
                case U_LF:
                case U_VT:
                case U_FF:
                case U_NL:
                case U_LS:
                case U_PS:
                    copy_buffer_until_into_ustring(this, ustr, p);
                    goto eol;
                default:
                    ++p;
            }
        }
        copy_full_buffer_into_ustring(this, ustr);
        available = fill_buffer(this, error);
    }

    if (FALSE) {
eol:
        available = 1;
    }
    ++this->lineno;
    ustring_normalize(ustr, env_get_normalization());

    return available > 0;
}

UChar32 reader_readuchar32(reader_t *this, error_t **error) /* NONNULL(1) */
{
    require_else_return_val(NULL != this, -1);

    if ((this->utf16.end - this->utf16.ptr) < 2) {
        fill_buffer(this, error);
    }
    if (this->utf16.ptr < this->utf16.end) {
        UChar32 c;

        c = *this->utf16.ptr++;
        if (U16_IS_LEAD(c)) {
            if (this->utf16.ptr < this->utf16.end) {
                c = U16_GET_SUPPLEMENTARY(c, *this->utf16.ptr);
                ++this->utf16.ptr;
            } else {
                c = U_EOF;
            }
        }

        return c;
    } else {
        return U_EOF;
    }
}

int32_t reader_readuchars32(reader_t *this, error_t **error, UChar32 *buffer, int32_t maxLen) /* NONNULL(1, 3) */
{
    int32_t i, available;

    require_else_return_val(NULL != this, -1);
    require_else_return_val(NULL != buffer, -1);
    require_else_return_val(maxLen >= 1, -1);

#if NO_PHYSICAL_REWIND
    available = this->utf16.end - this->utf16.ptr;
    if (0 == available) {
        if ((available = fill_buffer(this, error)) < 1) {
            return -1;
        }
    }
#endif /* NO_PHYSICAL_REWIND */
    for (i = 0; i < maxLen; i++) {
        if ((this->utf16.end - this->utf16.ptr) < 2) {
#ifndef NO_PHYSICAL_REWIND
            if ((available = fill_buffer(this, error)) < 1) {
                if (-1 == available) {
                    i = -1;
                }
                break;
            }
#else
            break;
#endif /* !NO_PHYSICAL_REWIND */
        }
        buffer[i] = *this->utf16.ptr++;
        if (U_IS_LEAD(buffer[i])) {
            buffer[i] = U16_GET_SUPPLEMENTARY(buffer[i], *this->utf16.ptr);
            ++this->utf16.ptr;
        }
    }

    return i;
}

int32_t reader_readuchars(reader_t *this, error_t **error, UChar *buffer, int32_t maxLen) /* NONNULL(1, 3) */
{
    int32_t count, available, copyLen;

    require_else_return_val(NULL != this, -1);
    require_else_return_val(NULL != buffer, -1);
    require_else_return_val(maxLen >= 2, -1);

    copyLen = count = 0;
    do {
        available = this->utf16.end - this->utf16.ptr;
        if (available < 1) {
            if ((available = fill_buffer(this, error)) < 1) {
                if (-1 == available) {
                    count = -1;
                }
                break;
            }
        }
        copyLen = MIN(maxLen - count, available);
        if ((count + copyLen == maxLen) && U16_IS_LEAD(this->utf16.ptr[copyLen - 1])) {
            --copyLen;
            if (count + copyLen >= maxLen) {
                u_memcpy(buffer + count, this->utf16.ptr, copyLen);
                this->utf16.ptr += copyLen;
                count += copyLen;
                break;
            }
        }
        u_memcpy(buffer + count, this->utf16.ptr, copyLen);
        this->utf16.ptr += copyLen;
        count += copyLen;
    } while (copyLen > 0 && count < maxLen);

    return count;
}

/* ==================== private misc helpers ==================== */

static UBool is_binary_uchar(UChar32 c)
{
    return !u_isprint(c) && !u_isspace(c) && U_BS != c;
}

static UBool is_binary(UChar32 *buffer, size_t buffer_len) /* NONNULL(1) */
{
    UChar32 *p;

    require_else_return_false(NULL != buffer);

    for (p = buffer; 0 != *p; p++) {
        if (is_binary_uchar(*p)) {
            return TRUE;
        }
    }

    return ((size_t)(p - buffer)) < buffer_len;
}

static UBool reader_is_seekable(reader_t *this)
{
    require_else_return_false(NULL != this);

    return STDIN_FILENO != this->fd;
}

#ifndef NO_PHYSICAL_REWIND
static UBool reader_rewind(reader_t *this, error_t **error)
#else
static UBool reader_rewind(reader_t *this, error_t **UNUSED(error))
#endif /* !NO_PHYSICAL_REWIND */
{
    UBool ret;

    require_else_return_false(NULL != this);

#ifndef NO_PHYSICAL_REWIND
    this->byte.ptr = this->byte.buffer + this->signature_length;
    this->utf16.end = this->utf16.ptr = this->utf16.buffer;
    ret = this->imp->rewindTo(this->fp, error, this->signature_length);
#else
//     ucnv_reset(this->ucnv);
//     ucnv_resetToUnicode(this->ucnv);
    this->utf16.ptr = this->utf16.buffer;
    /**
     * ucnv_detectUnicodeSignature() documentation says:
     *
     * The caller can ucnv_open() a converter using the charset name. The first code unit (UChar) from the start of the stream will
     * be U+FEFF (the Unicode BOM/signature character) and can usually be ignored.
     *
     * For most Unicode charsets it is also possible to ignore the indicated number of initial stream bytes and start converting after
     * them. However,there are stateful Unicode charsets (UTF-7 and BOCU-1) for which this will not work. Therefore, it is best to
     * ignore the first output UChar instead of the input signature bytes.
     **/
    if (0 != this->signature_length) {
        ++this->utf16.ptr;
    }
    ret = TRUE;
#endif /* !NO_PHYSICAL_REWIND */

    return ret;
}

/* ==================== public implementation getter/setter ==================== */

const reader_imp_t *reader_get_by_name(const char *name) /* NONNULL() */
{
    const reader_imp_t **imp;

    require_else_return_null(NULL != name);

    for (imp = available_readers; NULL != *imp; imp++) {
        if (0 == strcmp((*imp)->name, name)) {
#ifdef DYNAMIC_READERS
            if ((*imp)->internal || (NULL != (*imp)->trydload && !(*imp)->trydload())) {
#else
            if ((*imp)->internal) {
#endif /* DYNAMIC_READERS */
                return NULL;
            } else {
                return *imp;
            }
        }
    }

    return NULL;
}

UBool reader_set_imp_by_name(reader_t *this, const char *name) /* NONNULL() */
{
    const reader_imp_t **imp;

    require_else_return_false(NULL != this);
    require_else_return_false(NULL != name);

    for (imp = available_readers; NULL != *imp; imp++) {
        if (0 == strcmp((*imp)->name, name)) {
#ifdef DYNAMIC_READERS
            if ((*imp)->internal || (NULL != (*imp)->trydload && !(*imp)->trydload())) {
#else
            if ((*imp)->internal) {
#endif /* DYNAMIC_READERS */
                return FALSE;
            } else {
                this->default_imp = this->imp = *imp;
                return TRUE;
            }
        }
    }

    return FALSE;
}

/* ==================== public misc setter ==================== */

void reader_set_binary_behavior(reader_t *this, int binbehave) /* NONNULL() */
{
    require_else_return(NULL != this);
    require_else_return(binbehave >= BIN_FILE_BIN && binbehave <= BIN_FILE_TEXT);

    this->binbehave = binbehave;
}

/* ==================== public encoding setters ==================== */

UBool reader_set_encoding(reader_t *this, error_t **error, const char *encoding) /* NONNULL(1) */
{
    UErrorCode status;

    require_else_return_false(NULL != this);

    status = U_ZERO_ERROR;
    if (NULL != this->ucnv) {
        ucnv_close(this->ucnv);
    }
    this->ucnv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
    }
    this->encoding = encoding;

    return U_SUCCESS(status);
}

void reader_set_default_encoding(reader_t *this, const char *encoding) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    this->default_encoding = encoding;
}

/* ==================== public get/set user data ==================== */

void *reader_get_user_data(reader_t *this) /* NONNULL() */
{
    require_else_return_null(NULL != this);

    return this->priv_user;
}

void reader_set_user_data(reader_t *this, void *data) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    this->priv_user = data;
}

/* ==================== public "hacks" for special cases ==================== */

UBool reader_open_stdin(reader_t *this, error_t **error) /* NONNULL(1) */
{
    UBool ret;

    require_else_return_false(NULL != this);

    reader_init(this, "stdio");

    ret = reader_open(this, error, "-");
    if (!reader_set_encoding(this, error, env_get_stdin_encoding())) { /* NULL <=> inherit system encoding */
        return FALSE;
    }
    debug("%s, file encoding = %s", this->sourcename, this->encoding);

    return ret;
}

UBool reader_open_string(reader_t *this, error_t **error, const char *string) /* NONNULL(1, 3) */
{
    require_else_return_false(NULL != this);
    require_else_return_false(NULL != string);

    reader_init(this, NULL);
    this->fd = 0;
    this->imp = &string_reader_imp;
    this->sourcename = "(string)";
    if (NULL == (this->fp = string_open(string, -1))) {
        return FALSE;
    }
    if (!reader_set_encoding(this, error, env_get_stdin_encoding())) { /* NULL <=> inherit system encoding */
        return FALSE;
    }
    debug("%s, file encoding = %s", this->sourcename, this->encoding);

    return TRUE;
}

/* ==================== public main interface ==================== */

UBool reader_eof(reader_t *this) /* NONNULL() */
{
    require_else_return_false(NULL != this);

    return this->imp->eof(this->fp) && this->utf16.end == this->utf16.ptr /*&& this->byte.end == this->byte.buffer*/;
}

void reader_init(reader_t *this, const char *name) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    this->fd = -1;
    this->fp = NULL;
    this->encoding = NULL;
    this->sourcename = NULL;
    this->default_encoding = env_get_inputs_encoding();
    if (NULL != name) {
        reader_set_imp_by_name(this, name);
    } else {
        this->default_imp = this->imp = NULL;
    }
    this->priv_user = NULL;
    this->binbehave = 0;
    this->signature_length = 0;
    this->size = 0;
    this->lineno = 0;
    this->binary = FALSE;
    this->ucnv = NULL;
    this->byte.limit = this->byte.buffer + CHAR_BUFFER_SIZE;
    this->utf16.limit = this->utf16.buffer + UCHAR_BUFFER_SIZE;
    this->byte.ptr = this->byte.end = this->byte.buffer;
    this->utf16.ptr = this->utf16.end = this->utf16.buffer;
}

void reader_close(reader_t *this) /* NONNULL() */
{
    require_else_return(NULL != this);

    if (NULL != this->imp->close && NULL != this->fp) {
        this->imp->close(this->fp);
    }
    this->fp = NULL;
    if (NULL != this->ucnv) {
        ucnv_close(this->ucnv);
        this->ucnv = NULL;
    }
    if (this->fd > 0 && STDIN_FILENO != this->fd) {
        close(this->fd);
    }
    this->fd = -1;
}

static void reader_destroy(reader_t *this) /* NONNULL() */
{
    require_else_return(NULL != this);

    reader_close(this);
    free(this);
}

reader_t *reader_new(const char *name)
{
    reader_t *this;

    this = mem_new(*this);
    reader_init(this, name);
    env_register_resource(this, (func_dtor_t) reader_destroy);

    return this;
}

UBool reader_open(reader_t *this, error_t **error, const char *filename) /* NONNULL(1, 3) */
{
    UErrorCode status;
    size_t buffer_len;
    const char *encoding;
    char buffer[MAX_ENC_REL_LEN + 1] = { 0 };

    require_else_return_false(NULL != this);
    require_else_return_false(NULL != filename);

    if (!strcmp("-", filename)) {
        this->sourcename = "(standard input)";
        this->imp = &stdio_reader_imp;
        this->fd = STDIN_FILENO;
    } else {
        this->imp = this->default_imp;
        this->sourcename = filename;
        if (-1 == (this->fd = open(filename, O_RDONLY))) {
            error_set(error, WARN, "can't open %s: %s", filename, strerror(errno));
            goto failed;
        }
#ifdef WITH_FTS
        if (skip_file(this->fd)) {
            goto failed;
        }
#endif /* WITH_FTS */
    }

    if (NULL == (this->fp = this->imp->dopen(error, this->fd, this->sourcename))) {
        goto failed;
    }

    //this->ucnv = NULL;
    //encoding = NULL;
    buffer_len = 0;
    encoding = env_get_inputs_encoding();
    status = U_ZERO_ERROR;
    this->lineno = 0;
    this->binary = FALSE;
    this->signature_length = 0;
    this->byte.ptr = this->byte.end = this->byte.buffer;
    this->utf16.ptr = this->utf16.end = this->utf16.buffer;

    if (reader_is_seekable(this)) {
        if ((buffer_len = this->imp->readBytes(this->fp, error, buffer, MAX_ENC_REL_LEN)) > 0) {
#ifdef NO_PHYSICAL_REWIND
            memcpy(this->byte.buffer, buffer, buffer_len);
            this->byte.end = this->byte.buffer + buffer_len;
#endif /* NO_PHYSICAL_REWIND */
            buffer[buffer_len] = '\0';
            encoding = ucnv_detectUnicodeSignature(buffer, buffer_len, &this->signature_length, &status);
            if (U_SUCCESS(status)) {
                if (NULL == encoding) {
                    int32_t confidence;
                    UCharsetDetector *csd;
                    const char *tmpencoding;
                    const UCharsetMatch *ucm;

                    csd = ucsdet_open(&status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_open");
                        goto failed;
                    }
                    ucsdet_setText(csd, buffer, buffer_len, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_setText");
                        ucsdet_close(csd);
                        goto failed;
                    }
                    ucm = ucsdet_detect(csd, &status);
                    if (NULL != ucm) {
                        if (U_FAILURE(status)) {
                            icu_error_set(error, WARN, status, "ucsdet_detect");
                            ucsdet_close(csd);
                            goto failed;
                        }
                        confidence = ucsdet_getConfidence(ucm, &status);
                        tmpencoding = ucsdet_getName(ucm, &status);
                        if (U_FAILURE(status)) {
                            icu_error_set(error, WARN, status, "ucsdet_getName");
                            ucsdet_close(csd);
                            goto failed;
                        }
                        if (confidence > MIN_CONFIDENCE) {
                            encoding = tmpencoding;
                            //debug("%s, confidence of " GREEN("%d%%") " for " YELLOW("%s"), filename, confidence, tmpencoding);
                        } else {
                            //debug("%s, confidence of " RED("%d%%") " for " YELLOW("%s"), filename, confidence, tmpencoding);
                            //encoding = "US-ASCII";
                        }
                    }
                    ucsdet_close(csd);
                }
                //this->encoding = encoding;
            } else {
                icu_error_set(error, WARN, status, "ucnv_detectUnicodeSignature");
                goto failed;
            }
        }
    }
    if (!reader_set_encoding(this, error, encoding)) {
        goto failed;
    }
    debug("%s, file encoding = %s", this->sourcename, this->encoding);
    if (reader_is_seekable(this)) {
#ifdef NO_PHYSICAL_REWIND
        {
            UChar *utf16Ptr;

            utf16Ptr = this->utf16.buffer;
            ucnv_toUnicode(
                this->ucnv,
                &utf16Ptr, this->utf16.limit,
                (const char **) &this->byte.ptr, this->byte.end,
                NULL,
                this->imp->eof(this->fp),
                &status
            );
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "ucnv_toUnicode");
                return FALSE;
            }
            this->utf16.end = utf16Ptr;
        }
#endif /* NO_PHYSICAL_REWIND */
        if (!reader_rewind(this, error)) {
            goto failed;
        }
        if (BIN_FILE_TEXT != this->binbehave) {
            int32_t ubuffer_len;
            UChar32 ubuffer[MAX_BIN_REL_LEN + 1];

            if (-1 == (ubuffer_len = reader_readuchars32(this, error, ubuffer, MAX_BIN_REL_LEN))) {
                goto failed;
            }
            ubuffer[ubuffer_len] = 0;
            this->binary = is_binary(ubuffer, ubuffer_len);
            debug("%s, binary file : %s", filename, this->binary ? RED("yes") : GREEN("no"));
            if (this->binary) {
                if (BIN_FILE_SKIP == this->binbehave) {
                    goto failed;
                }
            }
            if (!reader_rewind(this, error)) {
                goto failed;
            }
        }
    }

    return TRUE;
failed:
    reader_close(this);
    return FALSE;
}
