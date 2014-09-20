#include <sys/types.h>
#include <unistd.h>
#include <bzlib.h>
#include <errno.h>

#include "common.h"

typedef struct {
    UBool eof;
    FILE *f;
//     int fd;
    BZFILE *fp;
} BZIP2;

#ifdef DYNAMIC_READERS
static const char *(*DRNS(BZ2_bzerror))(BZFILE *, int *) = NULL;
static BZFILE *(*DRNS(BZ2_bzReadOpen))(int *, FILE *, int, int, void *, int) = NULL;
static int (*DRNS(BZ2_bzRead))(int *, BZFILE *, void *, int) = NULL;
static void (*DRNS(BZ2_bzReadClose))(int *, BZFILE *) = NULL;

static UBool bzip2_trydload(void)
{
    DL_HANDLE handle;

    handle = DL_LOAD("bz2", 1);
    if (!handle) {
        if (HAVE_DL_ERROR) {
            debug("failed loading bzip2: %s", DL_ERROR);
        } else {
            debug("failed loading bzip2");
        }
        return FALSE;
    }
    DL_GET_SYM(handle, DRNS(BZ2_bzerror), "BZ2_bzerror");
    DL_GET_SYM(handle, DRNS(BZ2_bzReadOpen), "BZ2_bzReadOpen");
    DL_GET_SYM(handle, DRNS(BZ2_bzRead), "BZ2_bzRead");
    DL_GET_SYM(handle, DRNS(BZ2_bzReadClose), "BZ2_bzReadClose");
    env_register_resource(handle, (func_dtor_t) DL_UNLOAD);

    return TRUE;
}
#endif /* DYNAMIC_READERS */

static void *bzip2_dopen(error_t **error, int fd, const char * const filename)
{
    BZIP2 *this;
    int bzerror;

    this = mem_new(*this);
    this->eof = FALSE;
//     this->fd = fd;
    if (STDIN_FILENO == fd) {
        if (feof(stdin)) {
            clearerr(stdin);
        }
        this->f = stdin;
    } else {
        if (NULL == (this->f = fdopen(fd, "rb"))) {
            error_set(error, WARN, "fdopen() failed on %s: %s", filename, strerror(errno));
            return NULL;
        }
    }
    if (NULL == (this->fp = DRNS(BZ2_bzReadOpen)(&bzerror, this->f, 0, 0, NULL, 0))) {
//     if (NULL == (this->fp = BZ2_bzdopen(this->fd, "rb"))) {
        error_set(error, WARN, "BZ2_bzReadOpen() failed on %s", filename);
//         error_set(error, WARN, "bzdopen() failed on %s", filename);
        return NULL;
    }

    return this;
}

static void bzip2_close(void *fp)
{
    BZIP2 *this;
    int bzerror;

    this = (BZIP2 *) fp;
    DRNS(BZ2_bzReadClose)(&bzerror, this->fp);
//     BZ2_bzclose(this->fp);
//     if (STDIN_FILENO != this->fd) {
    if (fileno(this->f) != STDIN_FILENO) {
//     if (this->fd != STDIN_FILENO) {
        fclose(this->f);
//         close(this->fd);
    }
    free(this);
}

static UBool bzip2_eof(void *fp)
{
    BZIP2 *this;

    this = (BZIP2 *) fp;

    return this->eof;
}

#ifndef NO_PHYSICAL_REWIND
# define SIG_MAX_LEN 5
static UBool bzip2_rewindTo(void *fp, error_t **error, int32_t signature_length)
{
    BZIP2 *this;
    int bzerror;
    char buffer[SIG_MAX_LEN];

    this = (BZIP2 *) fp;
    DRNS(BZ2_bzReadClose)(&bzerror, this->fp);
//     BZ2_bzclose(this->fp);
    this->eof = FALSE;
    if (0 != fseek(this->f, (long) signature_length, SEEK_SET)) {
//     if (lseek(this->fd, (long) signature_length, SEEK_SET) < 0) {
        error_set(error, WARN, "fseek() failed: %s", strerror(errno));
//         error_set(error, WARN, "lseek failed: %s", strerror(errno));
        return FALSE;
    }
    if (NULL == (this->fp = DRNS(BZ2_bzReadOpen)(&bzerror, this->f, 0, 0, NULL, 0))) {
//     if (NULL == (this->fp = BZ2_bzdopen(this->fd, "rb"))) {
        error_set(error, WARN, "bzip2 internal error from BZ2_bzReadOpen()");
//         error_set(error, WARN, "bzip2 internal error from bzdopen()");
        return FALSE;
    }
    if (signature_length > 0) {
        DRNS(BZ2_bzRead)(&bzerror, this->fp, buffer, signature_length);
        switch (bzerror) {
            case BZ_OK:
                break;
            case BZ_STREAM_END:
                this->eof = TRUE;
                break;
            default:
                error_set(error, WARN, "bzip2 internal error from BZ2_bzRead(): %s", DRNS(BZ2_bzerror)(this->fp, &bzerror));
                return FALSE;
        }
    }

    return TRUE;
}
#endif /* !NO_PHYSICAL_REWIND */

static int32_t bzip2_readBytes(void *fp, error_t **error, char *buffer, size_t max_len)
{
    BZIP2 *this;
    int32_t ret;
    int bzerror;

    this = (BZIP2 *) fp;
    if (this->eof) {
        return 0;
    }
    ret = DRNS(BZ2_bzRead)(&bzerror, this->fp, buffer, max_len);
//     ret = BZ2_bzread(this->fp, buffer, max_len);
    switch (bzerror) {
        case BZ_OK:
            break;
        case BZ_STREAM_END:
            this->eof = TRUE;
            break;
        default:
            error_set(error, WARN, "bzip2 internal error from BZ2_bzRead(): %s", DRNS(BZ2_bzerror)(this->fp, &bzerror));
            return -1;
    }

    return ret;
}

reader_imp_t bzip2_reader_imp =
{
    FALSE,
    "bzip2",
#if 0
    "BZh",
#endif
#ifdef DYNAMIC_READERS
    bzip2_trydload,
#endif /* DYNAMIC_READERS */
    bzip2_dopen,
    bzip2_close,
    bzip2_eof,
    bzip2_readBytes
#ifndef NO_PHYSICAL_REWIND
    , bzip2_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
