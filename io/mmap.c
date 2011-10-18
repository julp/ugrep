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
#include <errno.h>

#include "common.h"

typedef struct {
#ifdef _MSC_VER
    HANDLE fd;
#else
    int fd;
#endif /* _MSC_VER */
    size_t len;
    const char *start, *end, *ptr;
} MMAP;

static void *mmap_dopen(error_t **error, int fd, const char * const filename)
{
    MMAP *this;
    struct stat st;

    this = mem_new(*this);

    this->ptr = this->start = NULL;
    this->len = 0;
    this->fd = -1;

    if (-1 == (fstat(fd, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        goto free;
    }
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

static void mmap_close(void *fp)
{
    MMAP *this;

    this = (MMAP *) fp;
#ifdef _MSC_VER
    UnmapViewOfFile(this->start);
    CloseHandle(this->fd);
#else
    munmap((void *) this->start, this->len);
    close(this->fd);
#endif /* _MSC_VER */
    free(this);
}

// copy of string_eof
static UBool mmap_eof(void *fp)
{
    MMAP *this;

    this = (MMAP *) fp;

    return this->ptr >= this->end;
}

// copy of string_rewindTo
#ifndef NO_PHYSICAL_REWIND
static UBool mmap_rewindTo(void *fp, error_t **UNUSED(error), int32_t signature_length)
{
    MMAP *this;

    this = (MMAP *) fp;
    this->ptr = this->start + signature_length;

    return TRUE;
}
#endif /* !NO_PHYSICAL_REWIND */

// copy of string_readBytes
static int32_t mmap_readBytes(void *fp, error_t **UNUSED(error), char *buffer, size_t max_len)
{
    int n;
    MMAP *this;

    this = (MMAP *) fp;
    if ((size_t) (this->end - this->ptr) > max_len) {
        n = max_len;
    } else {
        n = this->end - this->ptr;
    }
    memcpy(buffer, this->ptr, n);
    this->ptr += n;

    return n;
}

reader_imp_t mmap_reader_imp =
{
    FALSE,
    "mmap",
    mmap_dopen,
    mmap_close,
    mmap_eof,
    mmap_readBytes
#ifndef NO_PHYSICAL_REWIND
    , mmap_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
