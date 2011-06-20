#ifdef _MSC_VER
# include <io.h>
# include <windows.h>
#else
# include <sys/mman.h>
#endif /* _MSC_VER */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "common.h"

/*#ifndef SIZE_T_MAX
#  define SIZE_T_MAX (~((size_t) 0))
#endif*/ /* !SIZE_T_MAX */

typedef struct {
#ifdef _MSC_VER
    HANDLE fd;
#else
    int fd;
#endif /* _MSC_VER */
    size_t len;
    char *start, *end, *ptr;
    UConverter *ucnv;
    UChar pendingCU;
} mmfd_t;

static void *mmap_open(error_t **error, const char *filename, int fd)
{
    mmfd_t *this = NULL;
    struct stat st;

    this = mem_new(*this);

    this->ptr = this->start = NULL;
    this->len = 0;
    this->ucnv = NULL;
    this->fd = -1;

    if (-1 == (fstat(fd, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        goto free;
    }
    /*if (st.st_size > SIZE_T_MAX) {
        error_set(error, WARN, "%s too big (size > %dz)", filename, SIZE_T_MAX);
        goto close;
    }*/
    if (!S_ISREG(st.st_mode)) {
        error_set(error, WARN, "%s is not a regular file", filename);
        goto close;
    }
    if (0 == (this->len = (size_t) st.st_size)) {
        this->start = NULL;
    } else {
#ifdef _MSC_VER
        if (NULL == (this->fd = CreateFileMapping((HANDLE) _get_osfhandle(fd), NULL, PAGE_READONLY, 0, 0, NULL))) {
            error_set(error, WARN, "CreateFileMapping failed on %s: ", filename);
            goto close;
        }
        if (NULL == (this->start = MapViewOfFile(this->fd, FILE_MAP_READ, 0, 0, 0))) {
            CloseHandle(this->fd);
            error_win32_set(error, WARN, "MapViewOfFile failed on %s: ", filename);
            goto close;
        }
#else
        this->fd = fd;
        this->start = mmap(NULL, this->len, PROT_READ, MAP_PRIVATE, this->fd, (off_t) 0);
        if (MAP_FAILED == this->start) {
            error_set(error, WARN, "mmap failed on %s: %s", filename, strerror(errno));
            goto close;
        }
#endif /* _MSC_VER */
    }

    this->ptr = this->start;
    this->end = this->start + this->len;
    this->ucnv = NULL;
    this->pendingCU = 0;

    return this;

close:
#ifdef _MSC_VER
    CloseHandle(this->fd);
#else
    close(this->fd);
#endif /* _MSC_VER */
free:
    free(this);
    return NULL;
}

static void mmap_close(void *data)
{
    FETCH_DATA(data, this, mmfd_t);

    if (NULL != this->ucnv) {
        ucnv_close(this->ucnv);
    }
#ifdef _MSC_VER
    UnmapViewOfFile(this->start);
    CloseHandle(this->fd);
#else
    munmap(this->start, this->len);
    close(this->fd);
#endif /* _MSC_VER */
}

static int32_t mmap_readuchars(error_t **error, void *data, UChar *buffer, size_t max_len)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_READUCHARS(error, this->ucnv, this->ptr, this->end, buffer, max_len, this->pendingCU);
}

static int32_t mmap_readuchars32(error_t **error, void *data, UChar32 *buffer, size_t max_len)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_READUCHARS32(error, this->ucnv, this->ptr, this->end, buffer, max_len);
}

static void mmap_rewind(void *data, int32_t signature_length)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_REWIND(this->start, this->ptr, signature_length, this->pendingCU);
}

static UBool mmap_readline(error_t **error, void *data, UString *ustr)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_READLINE(error, this->ucnv, this->ptr, this->end, ustr);
}

static size_t mmap_readbytes(void *data, char *buffer, size_t max_len)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_READBYTES(this->ptr, this->end, buffer, max_len);
}

static UBool mmap_has_encoding(void *data)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_HAS_ENCODING(this->ucnv);
}

static const char *mmap_get_encoding(error_t **error, void *data)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_GET_ENCODING(error, this->ucnv);
}

static UBool mmap_set_encoding(error_t **error, void *data, const char *encoding)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_SET_ENCODING(error, this->ucnv, encoding);
}

static UBool mmap_eof(void *data)
{
    FETCH_DATA(data, this, mmfd_t);

    STRING_EOF(this->ptr, this->end, this->pendingCU);
}

static UBool mmap_seekable(void *UNUSED(data))
{
    STRING_SEEKABLE();
}


reader_imp_t mmap_reader_imp =
{
    FALSE,
    "mmap",
    mmap_open,
    mmap_close,
    mmap_eof,
    mmap_seekable,
    mmap_readline,
    mmap_readbytes,
    mmap_readuchars,
    mmap_readuchars32,
    mmap_has_encoding,
    mmap_get_encoding,
    mmap_set_encoding,
    mmap_rewind
};
