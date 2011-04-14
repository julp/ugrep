#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include "unistd.h"
#include "dirent.h"
#include "hash.h"

#define POINTER_TO_INT(p) ((int) (long) (p))
#define INT_TO_POINTER(i) ((void *) (long) (i))

#include <errno.h>

#ifndef EWOULDBLOCK
# define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef EINPROGRESS
# define EINPROGRESS WSAEINPROGRESS
#endif
#ifndef EALREADY
# define EALREADY WSAEALREADY
#endif
#ifndef ENOTSOCK
# define ENOTSOCK WSAENOTSOCK
#endif
#ifndef EDESTADDRREQ
# define EDESTADDRREQ WSAEDESTADDRREQ
#endif
#ifndef EMSGSIZE
# define EMSGSIZE WSAEMSGSIZE
#endif
#ifndef EPROTOTYPE
# define EPROTOTYPE WSAEPROTOTYPE
#endif
#ifndef ENOPROTOOPT
# define ENOPROTOOPT WSAENOPROTOOPT
#endif
#ifndef EPROTONOSUPPORT
# define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#endif
#ifndef ESOCKTNOSUPPORT
# define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#endif
#ifndef EOPNOTSUPP
# define EOPNOTSUPP WSAEOPNOTSUPP
#endif
#ifndef EPFNOSUPPORT
# define EPFNOSUPPORT WSAEPFNOSUPPORT
#endif
#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif
#ifndef EADDRINUSE
# define EADDRINUSE WSAEADDRINUSE
#endif
#ifndef EADDRNOTAVAIL
# define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#endif
#ifndef ENETDOWN
# define ENETDOWN WSAENETDOWN
#endif
#ifndef ENETUNREACH
# define ENETUNREACH WSAENETUNREACH
#endif
#ifndef ENETRESET
# define ENETRESET WSAENETRESET
#endif
#ifndef ECONNABORTED
# define ECONNABORTED WSAECONNABORTED
#endif
#ifndef ECONNRESET
# define ECONNRESET WSAECONNRESET
#endif
#ifndef ENOBUFS
# define ENOBUFS WSAENOBUFS
#endif
#ifndef EISCONN
# define EISCONN WSAEISCONN
#endif
#ifndef ENOTCONN
# define ENOTCONN WSAENOTCONN
#endif
#ifndef ESHUTDOWN
# define ESHUTDOWN WSAESHUTDOWN
#endif
#ifndef ETOOMANYREFS
# define ETOOMANYREFS WSAETOOMANYREFS
#endif
#ifndef ETIMEDOUT
# define ETIMEDOUT WSAETIMEDOUT
#endif
#ifndef ECONNREFUSED
# define ECONNREFUSED WSAECONNREFUSED
#endif
#ifndef ELOOP
# define ELOOP WSAELOOP
#endif
#ifndef EHOSTDOWN
# define EHOSTDOWN WSAEHOSTDOWN
#endif
#ifndef EHOSTUNREACH
# define EHOSTUNREACH WSAEHOSTUNREACH
#endif
#ifndef EPROCLIM
# define EPROCLIM WSAEPROCLIM
#endif
#ifndef EUSERS
# define EUSERS WSAEUSERS
#endif
#ifndef EDQUOT
# define EDQUOT WSAEDQUOT
#endif
#ifndef ESTALE
# define ESTALE WSAESTALE
#endif
#ifndef EREMOTE
# define EREMOTE WSAEREMOTE
#endif

#ifndef ERROR_PIPE_LOCAL
# define ERROR_PIPE_LOCAL 229L
#endif

static struct {
    DWORD winerr;
    int err;
} errmap[] = {
    {ERROR_INVALID_FUNCTION,          EINVAL},
    {ERROR_FILE_NOT_FOUND,            ENOENT},
    {ERROR_PATH_NOT_FOUND,            ENOENT},
    {ERROR_TOO_MANY_OPEN_FILES,       EMFILE},
    {ERROR_ACCESS_DENIED,             EACCES},
    {ERROR_INVALID_HANDLE,            EBADF},
    {ERROR_ARENA_TRASHED,             ENOMEM},
    {ERROR_NOT_ENOUGH_MEMORY,         ENOMEM},
    {ERROR_INVALID_BLOCK,             ENOMEM},
    {ERROR_BAD_ENVIRONMENT,           E2BIG},
    {ERROR_BAD_FORMAT,                ENOEXEC},
    {ERROR_INVALID_ACCESS,            EINVAL},
    {ERROR_INVALID_DATA,              EINVAL},
    {ERROR_INVALID_DRIVE,             ENOENT},
    {ERROR_CURRENT_DIRECTORY,         EACCES},
    {ERROR_NOT_SAME_DEVICE,           EXDEV},
    {ERROR_NO_MORE_FILES,             ENOENT},
    {ERROR_WRITE_PROTECT,             EROFS},
    {ERROR_BAD_UNIT,                  ENODEV},
    {ERROR_NOT_READY,                 ENXIO},
    {ERROR_BAD_COMMAND,               EACCES},
    {ERROR_CRC,                       EACCES},
    {ERROR_BAD_LENGTH,                EACCES},
    {ERROR_SEEK,                      EIO},
    {ERROR_NOT_DOS_DISK,              EACCES},
    {ERROR_SECTOR_NOT_FOUND,          EACCES},
    {ERROR_OUT_OF_PAPER,              EACCES},
    {ERROR_WRITE_FAULT,               EIO},
    {ERROR_READ_FAULT,                EIO},
    {ERROR_GEN_FAILURE,               EACCES},
    {ERROR_LOCK_VIOLATION,            EACCES},
    {ERROR_SHARING_VIOLATION,         EACCES},
    {ERROR_WRONG_DISK,                EACCES},
    {ERROR_SHARING_BUFFER_EXCEEDED,   EACCES},
    {ERROR_BAD_NETPATH,               ENOENT},
    {ERROR_NETWORK_ACCESS_DENIED,     EACCES},
    {ERROR_BAD_NET_NAME,              ENOENT},
    {ERROR_FILE_EXISTS,               EEXIST},
    {ERROR_CANNOT_MAKE,               EACCES},
    {ERROR_FAIL_I24,                  EACCES},
    {ERROR_INVALID_PARAMETER,         EINVAL},
    {ERROR_NO_PROC_SLOTS,             EAGAIN},
    {ERROR_DRIVE_LOCKED,              EACCES},
    {ERROR_BROKEN_PIPE,               EPIPE},
    {ERROR_DISK_FULL,                 ENOSPC},
    {ERROR_INVALID_TARGET_HANDLE,     EBADF},
    {ERROR_INVALID_HANDLE,            EINVAL},
    {ERROR_WAIT_NO_CHILDREN,          ECHILD},
    {ERROR_CHILD_NOT_COMPLETE,        ECHILD},
    {ERROR_DIRECT_ACCESS_HANDLE,      EBADF},
    {ERROR_NEGATIVE_SEEK,             EINVAL},
    {ERROR_SEEK_ON_DEVICE,            EACCES},
    {ERROR_DIR_NOT_EMPTY,             ENOTEMPTY},
    {ERROR_DIRECTORY,                 ENOTDIR},
    {ERROR_NOT_LOCKED,                EACCES},
    {ERROR_BAD_PATHNAME,              ENOENT},
    {ERROR_MAX_THRDS_REACHED,         EAGAIN},
    {ERROR_LOCK_FAILED,               EACCES},
    {ERROR_ALREADY_EXISTS,            EEXIST},
    {ERROR_INVALID_STARTING_CODESEG,  ENOEXEC},
    {ERROR_INVALID_STACKSEG,          ENOEXEC},
    {ERROR_INVALID_MODULETYPE,        ENOEXEC},
    {ERROR_INVALID_EXE_SIGNATURE,     ENOEXEC},
    {ERROR_EXE_MARKED_INVALID,        ENOEXEC},
    {ERROR_BAD_EXE_FORMAT,            ENOEXEC},
    {ERROR_ITERATED_DATA_EXCEEDS_64k, ENOEXEC},
    {ERROR_INVALID_MINALLOCSIZE,      ENOEXEC},
    {ERROR_DYNLINK_FROM_INVALID_RING, ENOEXEC},
    {ERROR_IOPL_NOT_ENABLED,          ENOEXEC},
    {ERROR_INVALID_SEGDPL,            ENOEXEC},
    {ERROR_AUTODATASEG_EXCEEDS_64k,   ENOEXEC},
    {ERROR_RING2SEG_MUST_BE_MOVABLE,  ENOEXEC},
    {ERROR_RELOC_CHAIN_XEEDS_SEGLIM,  ENOEXEC},
    {ERROR_INFLOOP_IN_RELOC_CHAIN,    ENOEXEC},
    {ERROR_FILENAME_EXCED_RANGE,      ENOENT},
    {ERROR_NESTING_NOT_ALLOWED,       EAGAIN},
    {ERROR_PIPE_LOCAL,                EPIPE},
    {ERROR_BAD_PIPE,                  EPIPE},
    {ERROR_PIPE_BUSY,                 EAGAIN},
    {ERROR_NO_DATA,                   EPIPE},
    {ERROR_PIPE_NOT_CONNECTED,        EPIPE},
    {ERROR_OPERATION_ABORTED,         EINTR},
    {ERROR_NOT_ENOUGH_QUOTA,          ENOMEM},
    {ERROR_MOD_NOT_FOUND,             ENOENT},
    {WSAEINTR,                        EINTR},
    {WSAEBADF,                        EBADF},
    {WSAEACCES,                       EACCES},
    {WSAEFAULT,                       EFAULT},
    {WSAEINVAL,                       EINVAL},
    {WSAEMFILE,                       EMFILE},
    {WSAEWOULDBLOCK,                  EWOULDBLOCK},
    {WSAEINPROGRESS,                  EINPROGRESS},
    {WSAEALREADY,                     EALREADY},
    {WSAENOTSOCK,                     ENOTSOCK},
    {WSAEDESTADDRREQ,                 EDESTADDRREQ},
    {WSAEMSGSIZE,                     EMSGSIZE},
    {WSAEPROTOTYPE,                   EPROTOTYPE},
    {WSAENOPROTOOPT,                  ENOPROTOOPT},
    {WSAEPROTONOSUPPORT,              EPROTONOSUPPORT},
    {WSAESOCKTNOSUPPORT,              ESOCKTNOSUPPORT},
    {WSAEOPNOTSUPP,                   EOPNOTSUPP},
    {WSAEPFNOSUPPORT,                 EPFNOSUPPORT},
    {WSAEAFNOSUPPORT,                 EAFNOSUPPORT},
    {WSAEADDRINUSE,                   EADDRINUSE},
    {WSAEADDRNOTAVAIL,                EADDRNOTAVAIL},
    {WSAENETDOWN,                     ENETDOWN},
    {WSAENETUNREACH,                  ENETUNREACH},
    {WSAENETRESET,                    ENETRESET},
    {WSAECONNABORTED,                 ECONNABORTED},
    {WSAECONNRESET,                   ECONNRESET},
    {WSAENOBUFS,                      ENOBUFS},
    {WSAEISCONN,                      EISCONN},
    {WSAENOTCONN,                     ENOTCONN},
    {WSAESHUTDOWN,                    ESHUTDOWN},
    {WSAETOOMANYREFS,                 ETOOMANYREFS},
    {WSAETIMEDOUT,                    ETIMEDOUT},
    {WSAECONNREFUSED,                 ECONNREFUSED},
    {WSAELOOP,                        ELOOP},
    {WSAENAMETOOLONG,                 ENAMETOOLONG},
    {WSAEHOSTDOWN,                    EHOSTDOWN},
    {WSAEHOSTUNREACH,                 EHOSTUNREACH},
    {WSAEPROCLIM,                     EPROCLIM},
    {WSAENOTEMPTY,                    ENOTEMPTY},
    {WSAEUSERS,                       EUSERS},
    {WSAEDQUOT,                       EDQUOT},
    {WSAESTALE,                       ESTALE},
    {WSAEREMOTE,                      EREMOTE}/*,*/
};

static int map_errno(DWORD winerr)
{
    int i;

    if (winerr == 0) {
        return 0;
    }

    for (i = 0; i < (int)(sizeof(errmap) / sizeof(*errmap)); i++) {
        if (errmap[i].winerr == winerr) {
            return errmap[i].err;
        }
    }

    if (winerr >= WSABASEERR) {
        return winerr;
    }

    return EINVAL;
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
        char *copy = strdup(full);
        *fd = --FD_FROM_P(x);
        return hashtable_put(HT_FROM_P(x), INT_TO_POINTER(*fd), copy);
    }
}

static void unregister_fd(void *x, int fd)
{
    hashtable_delete(HT_FROM_P(x), INT_TO_POINTER(fd));
}

static int fd_equal(const void *a, const void *b)
{
    return POINTER_TO_INT(a) == POINTER_TO_INT(b);
}

static int32_t fd_hash(const void *p)
{
    return POINTER_TO_INT(p);
}

void *dir_init()
{
    struct p *x;

    x = mem_new(*x);
    x->ht = hashtable_new(fd_hash, fd_equal, NULL, free);
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

        if (hashtable_get(HT_FROM_P(x), INT_TO_POINTER(fd), &ptr)) {
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
    int index, fd;
    char *filespec;
    struct _stat buffer;

    if (-1 == (fd = win32_open_ex(x, filename, _O_RDONLY, &buffer))) {
        return NULL;
    }
    if (!S_ISDIR(buffer.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    filespec = mem_new_n(*filename, strlen(filename) + 2 + 1);
    strcpy(filespec, filename);
    index = strlen(filespec) - 1;
    if (index >= 0 && (filespec[index] == '/' || (filespec[index] == '\\' && (index == 0 || !IsDBCSLeadByte(filespec[index-1]))))) {
        filespec[index] = '\0';
    }
    strcat(filespec, "\\*");

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
    dp->dir = strdup(filename);
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
        errno = map_errno(GetLastError());
        return -1;
    }
    
    return 0;
}
