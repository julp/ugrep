#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ugrep.h"

typedef struct {
    UConverter *ucnv;
    char *start, *base, *ptr, *end;
    size_t len;
} compressedfd_t;

#ifdef HAVE_ZLIB

# include <zlib.h>

static void *compressedfdgz_open(error_t **error, const char *filename, int fd)
{
    int ret;
    gzFile zfp;
    char *dst;
    size_t dst_len;
    struct stat st;
    compressedfd_t *compressedfd;

    compressedfd = mem_new(*compressedfd);
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
    compressedfd->base = compressedfd->start = compressedfd->ptr = dst;
    compressedfd->len = ret;
    compressedfd->end = compressedfd->start + compressedfd->len;
    compressedfd->ucnv = NULL;

    return compressedfd;

close:
    close(fd);
free:
    free(compressedfd);
    return NULL;
}

#endif /* HAVE_ZLIB */

#ifdef HAVE_BZIP2

# include <bzlib.h>

static void *compressedfdbz2_open(error_t **error, const char *filename, int fd)
{
    char *dst;
    BZFILE *jfp;
    size_t dst_len;
    struct stat st;
    int ret, bzerror;
    compressedfd_t *compressedfd;

    compressedfd = mem_new(*compressedfd);
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
    compressedfd->base = compressedfd->start = compressedfd->ptr = dst;
    compressedfd->len = ret;
    compressedfd->end = compressedfd->start + compressedfd->len;
    compressedfd->ucnv = NULL;

    return compressedfd;

close:
    close(fd);
free:
    free(compressedfd);
    return NULL;
}

#endif /* HAVE_BZLIP2 */

static void compressedfd_close(void *data)
{
    FETCH_DATA(data, compressedfd, compressedfd_t);

    if (NULL != compressedfd->ucnv) {
        ucnv_close(compressedfd->ucnv);
    }
    free(compressedfd->start);
}

static size_t compressedfd_readuchars(void *data, UChar32 *buffer, size_t max_len)
{
    UChar32 c;
    size_t i, len;
    UErrorCode status;
    FETCH_DATA(data, compressedfd, compressedfd_t);

    status = U_ZERO_ERROR;
    len = compressedfd->len > max_len ? max_len : compressedfd->len;
    for (i = 0; i < len; i++) {
        c = ucnv_getNextUChar(compressedfd->ucnv, (const char **) &compressedfd->ptr, compressedfd->end, &status);
        if (U_FAILURE(status)) {
            if (U_INDEX_OUTOFBOUNDS_ERROR == status) {
                break;
            } else {
                icu(status, "ucnv_getNextUChar");
                return 0;
            }
        }
        buffer[i] = c;
    }
    //buffer[i + 1] = U_NUL;

    return i;
}

static void compressedfd_rewind(void *data)
{
    FETCH_DATA(data, compressedfd, compressedfd_t);

    compressedfd->ptr = compressedfd->base;
}

static UBool compressedfd_readline(void *data, UString *ustr)
{
    UChar32 c;
    UErrorCode status;
    FETCH_DATA(data, compressedfd, compressedfd_t);

    status = U_ZERO_ERROR;
    do {
        c = ucnv_getNextUChar(compressedfd->ucnv, (const char **) &compressedfd->ptr, compressedfd->end, &status);
        if (U_FAILURE(status)) {
            if (U_INDEX_OUTOFBOUNDS_ERROR == status) { // c == U_EOF
                /*if (!ustring_empty(ustr)) {
                    break;
                } else {
                    return FALSE;
                }*/
                break;
            } else {
                icu(status, "ucnv_getNextUChar");
                return FALSE;
            }
        }
        ustring_append_char(ustr, c);
    } while (U_LF != c);

    return TRUE;
}

static size_t compressedfd_readbytes(void *data, char *buffer, size_t max_len)
{
    size_t n;
    FETCH_DATA(data, compressedfd, compressedfd_t);

    if (compressedfd->len > max_len) {
        n = max_len;
    } else {
        n = compressedfd->len;
    }
    memcpy(buffer, compressedfd->ptr, n);
    buffer[n + 1] = '\0';

    return n;
}

static UBool compressedfd_set_encoding(error_t **error, void *data, const char *encoding)
{
    UErrorCode status;
    FETCH_DATA(data, compressedfd, compressedfd_t);

    status = U_ZERO_ERROR;
    compressedfd->ucnv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
    }

    return U_SUCCESS(status);
}

static void compressedfd_set_signature_length(void *data, size_t signature_length)
{
    FETCH_DATA(data, compressedfd, compressedfd_t);

    compressedfd->len -= signature_length;
    compressedfd->base += signature_length;
}

static UBool compressedfd_eof(void *data)
{
    FETCH_DATA(data, compressedfd, compressedfd_t);

    return compressedfd->ptr >= compressedfd->end;
}

static UBool compressedfd_seekable(void *UNUSED(data))
{
    return TRUE;
}

#ifdef HAVE_ZLIB
reader_t gz_reader =
{
    "gzip",
    compressedfdgz_open,
    compressedfd_close,
    compressedfd_eof,
    compressedfd_seekable,
    compressedfd_readline,
    compressedfd_readbytes,
    compressedfd_readuchars,
    compressedfd_set_signature_length,
    compressedfd_set_encoding,
    compressedfd_rewind
};
#endif /* HAVE_ZLIB */

#ifdef HAVE_BZIP2
reader_t bz2_reader =
{
    "bzip2",
    compressedfdbz2_open,
    compressedfd_close,
    compressedfd_eof,
    compressedfd_seekable,
    compressedfd_readline,
    compressedfd_readbytes,
    compressedfd_readuchars,
    compressedfd_set_signature_length,
    compressedfd_set_encoding,
    compressedfd_rewind
};
#endif /* HAVE_BZIP2 */

