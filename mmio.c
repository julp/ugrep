#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "ugrep.h"

#ifndef SIZE_T_MAX
#  define SIZE_T_MAX (~((size_t) 0))
#endif /* !SIZE_T_MAX */

typedef struct {
    int fd;
    size_t len;
    char *base, *end, *ptr;
    UConverter *ucnv;
} mmfd_t;

static void *mmfd_open(error_t **error, const char *filename, int fd)
{
    mmfd_t *mmfd;
    struct stat st;

    mmfd = mem_new(*mmfd);
    mmfd->fd = fd;
    if (-1 == (fstat(mmfd->fd, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        goto free;
    }
    if (st.st_size > SIZE_T_MAX) {
        error_set(error, WARN, "%s too big (size > %dz)", filename, SIZE_T_MAX);
        goto close;
    }
    if (!S_ISREG(st.st_mode)) {
        error_set(error, WARN, "%s is not a regular file", filename);
        goto close;
    }
    mmfd->len = (size_t) st.st_size;
    mmfd->base = mmap(NULL, mmfd->len, PROT_READ, MAP_PRIVATE, mmfd->fd, (off_t) 0);
    if (MAP_FAILED == mmfd->base) {
        error_set(error, WARN, "mmap failed on %s: %s", filename, strerror(errno));
        goto close;
    }
    mmfd->ptr = mmfd->base;
    mmfd->end = mmfd->base + mmfd->len;
    mmfd->ucnv = NULL;

    return mmfd;

close:
    close(mmfd->fd);
free:
    free(mmfd);
    return NULL;
}

static void mmfd_close(void *data)
{
    FETCH_DATA(data, mmfd, mmfd_t);

    if (NULL != mmfd->ucnv) {
        ucnv_close(mmfd->ucnv);
    }
    munmap(mmfd->base, mmfd->len);
    close(mmfd->fd);
}

static size_t mmfd_readuchars(void *data, UChar32 *buffer, size_t max_len)
{
    UChar32 c;
    size_t i, len;
    UErrorCode status;
    FETCH_DATA(data, mmfd, mmfd_t);

    status = U_ZERO_ERROR;
    len = mmfd->len > max_len ? max_len : mmfd->len;
    for (i = 0; i < len; i++) {
        c = ucnv_getNextUChar(mmfd->ucnv, (const char **) &mmfd->ptr, mmfd->end, &status);
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

static void mmfd_rewind(void *data)
{
    FETCH_DATA(data, mmfd, mmfd_t);

    mmfd->ptr = mmfd->base;
}

static UBool mmfd_readline(void *data, UString *ustr)
{
    UChar32 c;
    UErrorCode status;
    FETCH_DATA(data, mmfd, mmfd_t);

    status = U_ZERO_ERROR;
    do {
        c = ucnv_getNextUChar(mmfd->ucnv, (const char **) &mmfd->ptr, mmfd->end, &status);
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

static size_t mmfd_readbytes(void *data, char *buffer, size_t max_len)
{
    size_t n;
    FETCH_DATA(data, mmfd, mmfd_t);

    if (mmfd->len > max_len) {
        n = max_len;
    } else {
        n = mmfd->len;
    }
    memcpy(buffer, mmfd->ptr, n);
    buffer[n + 1] = '\0';

    return n;
}

static void/*UBool*/ mmfd_set_encoding(void *data, const char *encoding)
{
    UErrorCode status;
    FETCH_DATA(data, mmfd, mmfd_t);

    mmfd->ucnv = ucnv_open(encoding, &status);
    if (U_FAILURE(status)) {
        icu(status, "ucnv_open");
    }

    //return U_SUCCESS(status);
}

static void mmfd_set_signature_length(void *data, size_t signature_length)
{
    FETCH_DATA(data, mmfd, mmfd_t);

    mmfd->len -= signature_length;
    mmfd->ptr = mmfd->base += signature_length;
}

static UBool mmfd_eof(void *data)
{
    FETCH_DATA(data, mmfd, mmfd_t);

    return mmfd->ptr >= mmfd->end;
}

static UBool mmfd_seekable(void *UNUSED(data))
{
    return TRUE;
}

reader_t mm_reader =
{
    "mmap",
    mmfd_open,
    mmfd_close,
    mmfd_eof,
    mmfd_seekable,
    mmfd_readline,
    mmfd_readbytes,
    mmfd_readuchars,
    mmfd_set_signature_length,
    mmfd_set_encoding,
    mmfd_rewind
};
