#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unicode/ucsdet.h>

#include "common.h"

#define MIN_CONFIDENCE  39   // Minimum confidence for a match (in percents)
#define MAX_ENC_REL_LEN 4096 // Maximum relevant length for encoding analyse (in bytes)
#define MAX_BIN_REL_LEN 4096 // Maximum relevant length for binary analyse

extern reader_imp_t mmap_reader_imp;
extern reader_imp_t stdio_reader_imp;
extern reader_imp_t string_reader_imp;
# ifdef HAVE_ZLIB
extern reader_imp_t gz_reader_imp;
# endif /* HAVE_ZLIB */
# ifdef HAVE_BZIP2
extern reader_imp_t bz2_reader_imp;
# endif /* HAVE_BZIP2 */

reader_imp_t *available_readers[] = {
    &mmap_reader_imp,
    &stdio_reader_imp,
#  ifdef HAVE_ZLIB
    &gz_reader_imp,
#  endif /* HAVE_ZLIB */
#  ifdef HAVE_BZIP2
    &bz2_reader_imp,
#  endif /* HAVE_BZIP2 */
    NULL
};

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

void reader_init(reader_t *this, const char *name) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    this->sourcename = NULL;
    this->default_encoding = util_get_inputs_encoding();
    if (NULL != name) {
        reader_set_imp_by_name(this, name);
    } else {
        this->default_imp = this->imp = NULL;
    }
    this->priv_imp = NULL;
    this->priv_user = NULL;
    this->binbehave = 0;
    this->signature_length = 0;
    this->size = 0;
    this->lineno = 0;
    this->binary = FALSE;
}

reader_imp_t *reader_get_by_name(const char *name)
{
    reader_imp_t **imp;

    for (imp = available_readers; *imp; imp++) {
        if (!(*imp)->internal && !strcmp((*imp)->name, name)) {
            return *imp;
        }
    }

    return NULL;
}

UBool reader_set_imp_by_name(reader_t *this, const char *name) /* NONNULL(1) */
{
    reader_imp_t **imp;

    require_else_return_false(NULL != this);

    for (imp = available_readers; *imp; imp++) {
        if (!(*imp)->internal && !strcmp((*imp)->name, name)) {
            this->default_imp = this->imp = *imp;
            return TRUE;
        }
    }

    return FALSE;
}

void reader_set_binary_behavior(reader_t *this, int binbehave) /* NONNULL(1) */
{
    require_else_return(NULL != this);
    require_else_return(binbehave >= BIN_FILE_BIN && binbehave <= BIN_FILE_TEXT);

    this->binbehave = binbehave;
}

UBool reader_set_encoding(reader_t *this, error_t **error, const char *encoding) /* NONNULL(1) */
{
    require_else_return_false(NULL != this);

    return this->imp->set_encoding(error, this->priv_imp, encoding);
}

void reader_set_default_encoding(reader_t *this, const char *encoding) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    this->default_encoding = encoding;
}

UBool reader_eof(reader_t *this) /* NONNULL(1) */
{
    require_else_return_false(NULL != this);

    return this->imp->eof(this->priv_imp);
}

int32_t reader_readuchars(reader_t *this, error_t **error, UChar *buffer, size_t max_len) /* NONNULL(1, 3) */
{
    require_else_return_val(NULL != this, -1);
    require_else_return_val(NULL != buffer, -1);

    return this->imp->readuchars(error, this->priv_imp, buffer, max_len);
}

UBool reader_readline(reader_t *this, error_t **error, UString *ustr) /* NONNULL(1, 3) */
{
    require_else_return_false(NULL != this);
    require_else_return_false(NULL != ustr);

    ustring_truncate(ustr);

    return this->imp->readline(error, this->priv_imp, ustr);
}

void reader_close(reader_t *this) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    if (NULL != this->imp->close) {
        this->imp->close(this->priv_imp);
    }
    free(this->priv_imp);
    this->priv_imp = NULL;
}

void *reader_get_user_data(reader_t *this) /* NONNULL(1) */
{
    require_else_return_null(NULL != this);

    return this->priv_user;
}

void reader_set_user_data(reader_t *this, void *data) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    this->priv_user = data;
}

UBool reader_open_stdin(reader_t *this, error_t **error) /* NONNULL(1) */
{
    require_else_return_false(NULL != this);

    reader_init(this, "stdio");

    return reader_open(this, error, "-");
}

UBool reader_open_string(reader_t *this, error_t **error, const char *string) /* NONNULL(1, 3) */
{
    require_else_return_false(NULL != this);
    require_else_return_false(NULL != string);

    reader_init(this, NULL);
    this->imp = &string_reader_imp;
    this->sourcename = "(string)";
    if (NULL == (this->priv_imp = this->imp->open(error, string, -1))) {
        return FALSE;
    }
    if (!this->imp->set_encoding(error, this->priv_imp, util_get_stdin_encoding())) { /* NULL <=> inherit system encoding */
        return FALSE;
    }

    return TRUE;
}

UBool reader_open(reader_t *this, error_t **error, const char *filename) /* NONNULL(1, 3) */
{
    int fsfd;
    /*struct stat st;*/
    UErrorCode status;
    size_t buffer_len;
    const char *encoding;
    char buffer[MAX_ENC_REL_LEN + 1];

    require_else_return_false(NULL != this);
    require_else_return_false(NULL != filename);

    /*if (-1 == (stat(filename, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        return FALSE;
    }*/

    if (!strcmp("-", filename)) {
        this->sourcename = "(standard input)";
        this->imp = &stdio_reader_imp;
        fsfd = STDIN_FILENO;
    } else {
        this->imp = this->default_imp;
        this->sourcename = filename;
        if (-1 == (fsfd = open(filename, O_RDONLY))) {
            error_set(error, WARN, "can't open %s: %s", filename, strerror(errno));
            goto failed;
        }
    }

    if (NULL == (this->priv_imp = this->imp->open(error, filename, fsfd))) {
        goto failed;
    }

    //encoding = NULL;
    encoding = util_get_inputs_encoding();
    status = U_ZERO_ERROR;
    this->lineno = 0;
    this->binary = FALSE;
    /*this->filesize = st.st_size;*/
    this->signature_length = 0;

    if (this->imp->seekable(this->priv_imp)) {
        if (0 != (buffer_len = this->imp->readbytes(this->priv_imp, buffer, MAX_ENC_REL_LEN))) {
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
                        goto failed;
                    }
                    ucm = ucsdet_detect(csd, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_detect");
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
                    ucsdet_close(csd);
                }
                debug("%s, file encoding = %s", filename, encoding);
                //this->encoding = encoding;
            } else {
                icu_error_set(error, WARN, status, "ucnv_detectUnicodeSignature");
                goto failed;
            }
        }
    }
    if (!this->imp->set_encoding(error, this->priv_imp, encoding)) {
        goto failed;
    }
#ifdef DEBUG
    if (NULL != (encoding = this->imp->get_encoding(NULL, this->priv_imp))) {
        debug("%s, file encoding = %s", filename, encoding);
    }
#endif /* DEBUG */
    if (this->imp->seekable(this->priv_imp)) {
        this->imp->rewind(this->priv_imp, this->signature_length);
        if (BIN_FILE_TEXT != this->binbehave) {
            int32_t ubuffer_len;
            UChar32 ubuffer[MAX_BIN_REL_LEN + 1];

            if (-1 == (ubuffer_len = this->imp->readuchars32(error, this->priv_imp, ubuffer, MAX_BIN_REL_LEN))) {
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
            this->imp->rewind(this->priv_imp, this->signature_length);
        }
    }

    return TRUE;
failed:
    if (NULL != this->priv_imp) {
        reader_close(this);
    }
    return FALSE;
}
