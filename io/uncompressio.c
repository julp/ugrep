#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"

typedef struct {
    UConverter *ucnv;
    char *start, *ptr, *end;
    size_t len;
} compressedfd_t;

#ifdef HAVE_ZLIB

# include <zlib.h>

static void *compressedgz_open(error_t **error, const char *filename, int fd)
{
    int ret;
    gzFile zfp;
    char *dst;
    size_t dst_len;
    struct stat st;
    compressedfd_t *this;

    this = mem_new(*this);
    if (-1 == (fstat(fd, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        goto close;
    }
    if (!S_ISREG(st.st_mode)) {
        error_set(error, WARN, "%s is not a regular file", filename);
        goto close;
    }
    if (NULL == (zfp = gzdopen(fd, "rb"))) {
        error_set(error, WARN, "gzdopen failed");
        goto close;
    }
    dst_len = 2 * st.st_size;
    dst = mem_new_n(*dst, dst_len + 1);
    if (-1 == (ret = gzread(zfp, dst, dst_len))) {
        int errnum;
        const char *zerrstr;

        zerrstr = gzerror(zfp, &errnum);
        if (Z_ERRNO == errnum) {
            error_set(error, WARN, "zlib external error from gzread(): %s", strerror(errno));
        } else {
            error_set(error, WARN, "zlib internal error from gzread(): %s", zerrstr);
        }
        gzclose(zfp);
        goto free;
    }
    gzclose(zfp);
    this->start = this->ptr = dst;
    this->len = ret;
    this->end = this->start + this->len;
    this->ucnv = NULL;

    return this;

close:
    close(fd);
free:
    free(this);
    return NULL;
}

#endif /* HAVE_ZLIB */

#ifdef HAVE_BZIP2

# include <bzlib.h>

static void *compressedbz2_open(error_t **error, const char *filename, int fd)
{
    char *dst;
    BZFILE *jfp;
    size_t dst_len;
    struct stat st;
    int ret, bzerror;
    compressedfd_t *this;

    this = mem_new(*this);
    if (-1 == (fstat(fd, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        goto close;
    }
    if (!S_ISREG(st.st_mode)) {
        error_set(error, WARN, "%s is not a regular file", filename);
        goto close;
    }
    if (NULL == (jfp = BZ2_bzdopen(fd, "rb"))) {
        error_set(error, WARN, "bzdopen failed");
        goto close;
    }
    dst_len = 2 * st.st_size;
    dst = mem_new_n(*dst, dst_len + 1);
    if (-1 == (ret = BZ2_bzRead(&bzerror, jfp, dst, dst_len))) {
        error_set(error, WARN, "bzip2 internal error from BZ2_bzread(): %s", BZ2_bzerror(jfp, &bzerror));
        BZ2_bzReadClose(&bzerror, jfp);
        goto free;
    }
    BZ2_bzReadClose(&bzerror, jfp);
    this->start = this->ptr = dst;
    this->len = ret;
    this->end = this->start + this->len;
    this->ucnv = NULL;

    return this;

close:
    close(fd);
free:
    free(this);
    return NULL;
}

#endif /* HAVE_BZLIP2 */

static void compressed_close(void *data)
{
    FETCH_DATA(data, this, compressedfd_t);

    if (NULL != this->ucnv) {
        ucnv_close(this->ucnv);
    }
    free(this->start);
}

static int32_t compressed_readuchars(error_t **error, void *data, UChar *buffer, size_t max_len)
{
    FETCH_DATA(data, this, compressedfd_t);

    STRING_READUCHARS(error, this->ptr, this->end, this->ucnv, buffer, max_len);
}

static int32_t compressed_readuchars32(error_t **error, void *data, UChar32 *buffer, size_t max_len)
{
    UChar32 c;
    int32_t i, len;
    UErrorCode status;
    FETCH_DATA(data, this, compressedfd_t);

    status = U_ZERO_ERROR;
    len = this->len > max_len ? max_len : this->len;
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

static void compressed_rewind(void *data, int32_t signature_length)
{
    FETCH_DATA(data, this, compressedfd_t);

    this->ptr = this->start + signature_length;
}

static UBool compressed_readline(error_t **error, void *data, UString *ustr)
{
    UChar32 c;
    UErrorCode status;
    FETCH_DATA(data, this, compressedfd_t);

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

static size_t compressed_readbytes(void *data, char *buffer, size_t max_len)
{
    size_t n;
    FETCH_DATA(data, this, compressedfd_t);

    if (this->len > max_len) {
        n = max_len;
    } else {
        n = this->len;
    }
    memcpy(buffer, this->ptr, n);

    return n;
}

static UBool compressed_has_encoding(void *data)
{
    FETCH_DATA(data, this, compressedfd_t);

    return NULL != this->ucnv;
}

static const char *compressed_get_encoding(error_t **error, void *data)
{
    FETCH_DATA(data, this, compressedfd_t);

    STRING_GET_ENCODING(error, this->ucnv);
}

static UBool compressed_set_encoding(error_t **error, void *data, const char *encoding)
{
    UErrorCode status;
    FETCH_DATA(data, this, compressedfd_t);

    status = U_ZERO_ERROR;
    this->ucnv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
    }

    return U_SUCCESS(status);
}

static UBool compressed_eof(void *data)
{
    FETCH_DATA(data, this, compressedfd_t);

    return this->ptr >= this->end;
}

static UBool compressed_seekable(void *UNUSED(data))
{
    return TRUE;
}


#ifdef HAVE_ZLIB
reader_imp_t gz_reader_imp =
{
    FALSE,
    "gzip",
    compressedgz_open,
    compressed_close,
    compressed_eof,
    compressed_seekable,
    compressed_readline,
    compressed_readbytes,
    compressed_readuchars,
    compressed_readuchars32,
    compressed_has_encoding,
    compressed_get_encoding,
    compressed_set_encoding,
    compressed_rewind
};
#endif /* HAVE_ZLIB */

#ifdef HAVE_BZIP2
reader_imp_t bz2_reader_imp =
{
    FALSE,
    "bzip2",
    compressedbz2_open,
    compressed_close,
    compressed_eof,
    compressed_seekable,
    compressed_readline,
    compressed_readbytes,
    compressed_readuchars,
    compressed_readuchars32,
    compressed_has_encoding,
    compressed_get_encoding,
    compressed_set_encoding,
    compressed_rewind
};
#endif /* HAVE_BZIP2 */
