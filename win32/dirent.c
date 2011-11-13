#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include "unistd.h"
#include "dirent.h"
#include "hashtable.h"

#define POINTER_TO_UINT(p) ((uint32_t) (p))
#define UINT_TO_POINTER(i) ((void *)   (i))

#include <errno.h>

static int getlasterror2errno(DWORD errcode)
{
    switch (errcode) {
        case ERROR_SUCCESS:
            return 0;

        case ERROR_FILE_TOO_LARGE:
            return EFBIG;

        case ERROR_TOO_MANY_OPEN_FILES:
            return EMFILE;

        case ERROR_BUSY:
        case ERROR_BUSY_DRIVE:
        case ERROR_PATH_BUSY:
            return EBUSY;

        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return ENOENT;

        case ERROR_ACCESS_DENIED:
            return EACCES;

        case ERROR_INVALID_HANDLE:
            return EBADF;

        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return ENOMEM;

        case ERROR_INVALID_NAME:
            return EINVAL;

        case ERROR_DIRECTORY:
            return ENOTDIR;

        case ERROR_LABEL_TOO_LONG:
        case ERROR_BUFFER_OVERFLOW:
            return ENAMETOOLONG;

        case ERROR_DIR_NOT_EMPTY:
            return ENOTEMPTY;

        case ERROR_SEEK:
        case ERROR_READ_FAULT:
        case ERROR_WRITE_FAULT:
            return EIO;

        case ERROR_WRITE_PROTECT:
            return EROFS;

        case ERROR_NEGATIVE_SEEK:
            return ESPIPE;

        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            return EEXIST;

        case ERROR_INVALID_FLAG_NUMBER:
        case ERROR_INVALID_PARAMETER:
        case ERROR_BAD_ARGUMENTS:
            return EINVAL;

        case ERROR_NO_DATA:
        case ERROR_PIPE_NOT_CONNECTED:
            return EPIPE;

        case ERROR_INVALID_ADDRESS:
            return EFAULT;

        default:
            debug("Unmapped code: %d (0x%X)", errcode, errcode);
            return EINVAL;
    }
}

struct _dir {
    HANDLE handle;
    short offset;
    short finished;
    WIN32_FIND_DATA fileinfo;
    char *dir;
    struct dirent dent;
    int fd;
};

struct p {
    int lastfd;
    Hashtable *ht;
};

#define HT_FROM_P(x) (((struct p*) x)->ht)
#define FD_FROM_P(x) (((struct p*) x)->lastfd)

static int register_fd(void *x, int *fd, const char *path)
{
    char full[_MAX_PATH];

    if (NULL == _fullpath(full, path, _MAX_PATH)) {
        return 0;
    } else {
        char *copy = _strdup(full);
        *fd = --FD_FROM_P(x);
        hashtable_put(HT_FROM_P(x), UINT_TO_POINTER(*fd), copy);
        return 1;
    }
}

static void unregister_fd(void *x, int fd)
{
    hashtable_remove(HT_FROM_P(x), UINT_TO_POINTER(fd));
}

static int fd_equal(const void *a, const void *b)
{
    return POINTER_TO_UINT(a) == POINTER_TO_UINT(b);
}

static uint32_t fd_hash(const void *p)
{
    return POINTER_TO_UINT(p);
}

void *dir_init()
{
    struct p *x;

    x = mem_new(*x);
    x->ht = hashtable_new(fd_hash, fd_equal, NULL, free, NODUP, NODUP);
    x->lastfd = -1;

    return (void *) x;
}

void dir_deinit(void *x)
{
    hashtable_destroy(HT_FROM_P(x));
    free(x);
}

int win32_open_ex(void *x, const char *filename, int oflag, struct stat *buffer)
{
    int fd;
    struct stat _buffer;

    if (NULL == buffer) {
        buffer = &_buffer;
    }
    if (0 != stat(filename, buffer)) {
        return -1;
    }
    if (S_ISDIR(buffer->st_mode)) {
        if (!register_fd(x, &fd, filename)) {
            return -1;
        }
    } else {
        if (-1 == (fd = _open(filename, _O_RDONLY))) {
            return -1;
        }
    }

    return fd;
}

int win32_open(void *x, const char *filename, int oflag)
{
    return win32_open_ex(x, filename, oflag, NULL);
}

int win32_fstat(void *x, int fd, struct stat *buffer)
{
    if (fd == -1) {
        errno = EBADF;
        return -1;
    } else if (fd >= 0) {
        return fstat(fd, buffer);
    } else {
        void *ptr;

        if (hashtable_get(HT_FROM_P(x), UINT_TO_POINTER(fd), &ptr)) {
            if (NULL == ptr) {
                errno = EBADF;
                return -1;
            } else {
                return stat(ptr, buffer);
            }
        } else {
            errno = EBADF;
            return -1;
        }
    }
}

int win32_close(void *x, int fd)
{
    unregister_fd(x, fd);
    if (fd >= 0) {
        return _close(fd);
    } else {
        return 0;
    }
}

DIR *opendir(void *x, const char *filename)
{
    DIR *dp;
    char *filespec;
    struct _stat buffer;
    int index, fd, allocated;

    if (-1 == (fd = win32_open_ex(x, filename, _O_RDONLY, &buffer))) {
        return NULL;
    }
    if (!S_ISDIR(buffer.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    allocated = strlen(filename) + 2 + 1;
    filespec = mem_new_n(*filename, allocated);
    if (0 != strcpy_s(filespec, allocated, filename)) {
        free(filespec);
        return NULL;
    }
    index = strlen(filespec) - 1;
    if (index >= 0 && (filespec[index] == '/' || (filespec[index] == '\\' && (index == 0 || !IsDBCSLeadByte(filespec[index-1]))))) {
        filespec[index] = '\0';
    }
    if (0 != strncat_s(filespec, allocated, "\\*", sizeof("\\*") - 1)) {
        free(filespec);
        return NULL;
    }

    dp = mem_new(*dp);
    dp->offset = 0;
    dp->finished = 0;

    if ((dp->handle = FindFirstFileA(filespec, &(dp->fileinfo))) == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_NO_MORE_FILES || err == ERROR_FILE_NOT_FOUND) {
            dp->finished = 1;
        } else {
            free(dp);
            free(filespec);
            win32_close(x, fd);
            return NULL;
        }
    }
    dp->fd = fd;
    dp->dir = _strdup(filename);
    free(filespec);

    return dp;
}

struct dirent *readdir(DIR *dirp)
{
    if (NULL == dirp || dirp->finished) {
        return NULL;
    }
    if (0 != dirp->offset) {
        if (0 == FindNextFileA(dirp->handle, &(dirp->fileinfo))) {
            dirp->finished = 1;
            return NULL;
        }
    }
    dirp->offset++;
    //strlcpy(dirp->dent.d_name, dirp->fileinfo.cFileName, _MAX_FNAME + 1);
    if (EINVAL == strcpy_s(dirp->dent.d_name, _MAX_FNAME + 1, dirp->fileinfo.cFileName)) {
        return NULL;
    }
    dirp->dent.d_ino = 1;
    dirp->dent.d_reclen = strlen(dirp->dent.d_name);
    dirp->dent.d_off = dirp->offset;
    dirp->dent.d_type = ((dirp->fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) ? DT_DIR : DT_REG;

    return &(dirp->dent);
}

int dirfd(DIR *dirp)
{
    return dirp->fd;
}

int closedir(DIR *dirp)
{
    if (NULL != dirp) {
        if (INVALID_HANDLE_VALUE != dirp->handle) {
            FindClose(dirp->handle);
        }
        if (NULL != dirp->dir) {
            free(dirp->dir);
        }
        if (NULL != dirp) {
            free(dirp);
        }
    }

    return 0;
}

int fchdir(void *x, int fd)
{
    void *ptr;

    if (hashtable_get(HT_FROM_P(x), INT_TO_POINTER(fd), &ptr)) {
        if (NULL == ptr) {
            // errno ?
            return -1;
        } else {
            return chdir(ptr);
        }
    } else {
        // errno ?
        return -1;
    }
}

int chdir(const char *path)
{
    if (!SetCurrentDirectoryA(path)) {
        errno = getlasterror2errno(GetLastError());
        return -1;
    }

    return 0;
}
