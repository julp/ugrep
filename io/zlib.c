#include <zlib.h>
#include <errno.h>

#include "common.h"

#define DEBUG_READS 1

#ifdef DYNAMIC_READERS
static const char *(*DRNS(gzerror))(gzFile, int *) = NULL;
#if 0
#ifdef _MSC_VER
static gzFile (*DRNS(gzopen))(const char *, const char *) = NULL;
#endif /* _MSC_VER */
#endif
static gzFile (*DRNS(gzdopen))(int, const char *) = NULL;
static int (*DRNS(gzread))(gzFile, voidp, unsigned) = NULL;
static int (*DRNS(gzclose))(gzFile) = NULL;
static int (*DRNS(gzeof))(gzFile) = NULL;
static z_off_t (*DRNS(gzseek))(gzFile, z_off_t, int) = NULL;

static UBool zlib_trydload(void)
{
    DL_HANDLE handle;

    handle = DL_LOAD("z", 1);
    if (!handle) {
        if (HAVE_DL_ERROR) {
            debug("failed loading zlib: %s", DL_ERROR);
        } else {
            debug("failed loading zlib");
        }
        return FALSE;
    }
    DL_GET_SYM(handle, DRNS(gzerror), "gzerror");
    DL_GET_SYM(handle, DRNS(gzdopen), "gzdopen");
#if 0
#ifdef _MSC_VER
    DL_GET_SYM(handle, DRNS(gzopen), "gzopen");
#endif /* _MSC_VER */
#endif
    DL_GET_SYM(handle, DRNS(gzread), "gzread");
    DL_GET_SYM(handle, DRNS(gzclose), "gzclose");
    DL_GET_SYM(handle, DRNS(gzeof), "gzeof");
    DL_GET_SYM(handle, DRNS(gzseek), "gzseek");
    env_register_resource(handle, (func_dtor_t) DL_UNLOAD);

    return TRUE;
}
#endif /* DYNAMIC_READERS */

static void *zlib_dopen(error_t **error, int fd, const char * const filename)
{
    gzFile fp;

    if (NULL == (fp = DRNS(gzdopen)(fd, "rb"))) {
        error_set(error, WARN, "gzdopen() failed on %s", filename);
    }
#if 0
#ifdef _MSC_VER
    if (STDIN_FILENO == fd) {
#endif /* _MSC_VER */
        if (NULL == (fp = DRNS(gzdopen)(fd, "rb"))) {
            error_set(error, WARN, "gzdopen failed on %s", filename);
        }
#ifdef _MSC_VER
    } else {
        if (NULL == (fp = DRNS(gzopen)(filename, "rb"))) {
            error_set(error, WARN, "gzopen failed on %s", filename);
        }
    }
#endif /* _MSC_VER */
#endif

    return fp;
}

static void zlib_close(void *fp)
{
    DRNS(gzclose)((gzFile) fp);
}

static UBool zlib_eof(void *fp)
{
#ifdef DEBUG_READS
    debug("eof = %d", DRNS(gzeof)((gzFile) fp));
#endif /* DEBUG_READS */
    return DRNS(gzeof)((gzFile) fp);
}

#ifndef NO_PHYSICAL_REWIND
static UBool zlib_rewindTo(void *fp, error_t **error, int32_t signature_length)
{
    if (signature_length != DRNS(gzseek)((gzFile) fp, signature_length, SEEK_SET)) {
        int errnum;
        const char *zerrstr;

        zerrstr = DRNS(gzerror)((gzFile) fp, &errnum);
        if (Z_ERRNO == errnum) {
            error_set(error, WARN, "zlib external error from gzseek(): %s", strerror(errno));
        } else {
            error_set(error, WARN, "zlib internal error from gzseek(): %s", zerrstr);
        }
        return FALSE;
    }

    return TRUE;
}
#endif /* !NO_PHYSICAL_REWIND */

static int32_t zlib_readBytes(void *fp, error_t **error, char *buffer, size_t max_len)
{
    int ret;

    if (-1 == (ret = DRNS(gzread)((gzFile) fp, buffer, max_len))) {
        int errnum;
        const char *zerrstr;

        zerrstr = DRNS(gzerror)((gzFile) fp, &errnum);
        if (Z_ERRNO == errnum) {
            error_set(error, WARN, "zlib external error from gzread(): %s", strerror(errno));
        } else {
            error_set(error, WARN, "zlib internal error from gzread(): %s", zerrstr);
        }
    }
#ifdef DEBUG_READS
    debug("asked = %d, get = %d", max_len, ret);
#endif /* DEBUG_READS */

    return ret;
}

reader_imp_t zlib_reader_imp =
{
    FALSE,
    "gzip",
#if 0
    "\x1F\x8B",
#endif
#ifdef DYNAMIC_READERS
    zlib_trydload,
#endif /* DYNAMIC_READERS */
    zlib_dopen,
    zlib_close,
    zlib_eof,
    zlib_readBytes
#ifndef NO_PHYSICAL_REWIND
    , zlib_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
