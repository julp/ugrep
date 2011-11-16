#include <zlib.h>
#include <errno.h>

#include "common.h"

#ifdef DYNAMIC_READERS
# define NS(name) dl##name
static const char *(*NS(gzerror))(gzFile, int *) = NULL;
static gzFile (*NS(gzdopen))(int, const char *) = NULL;
static int (*NS(gzread))(gzFile, voidp, unsigned) = NULL;
static int (*NS(gzclose))(gzFile) = NULL;
static int (*NS(gzeof))(gzFile) = NULL;
static z_off_t (*NS(gzseek))(gzFile, z_off_t, int) = NULL;

static UBool zlib_available(void)
{
    DL_HANDLE handle;

    handle = DL_LOAD("z", 1); // zlib1.dll vs libz.so.1
    if (!handle) {
#if HAVE_DL_ERROR
        stdio_debug("failed loading zlib: %s", DL_ERROR);
#else
        stdio_debug("failed loading zlib");
#endif /* DL_ERROR */
        return FALSE;
    }
    DL_GET_SYM(handle, NS(gzerror), "gzerror");
    DL_GET_SYM(handle, NS(gzdopen), "gzdopen");
    DL_GET_SYM(handle, NS(gzread), "gzread");
    DL_GET_SYM(handle, NS(gzclose), "gzclose");
    DL_GET_SYM(handle, NS(gzeof), "gzeof");
    DL_GET_SYM(handle, NS(gzseek), "gzseek");

    return TRUE;
}
#else
# define NS(name) name
#endif /* DYNAMIC_READERS */

static void *zlib_dopen(error_t **error, int fd, const char * const filename)
{
    gzFile fp;

    if (NULL == (fp = NS(gzdopen)(fd, "rb"))) {
        error_set(error, WARN, "gzdopen failed on %s", filename);
    }

    return fp;
}

static void zlib_close(void *fp)
{
    NS(gzclose)((gzFile) fp);
}

static UBool zlib_eof(void *fp)
{
// debug("eof = %d", gzeof((gzFile) fp));
    return NS(gzeof)((gzFile) fp);
}

#ifndef NO_PHYSICAL_REWIND
static UBool zlib_rewindTo(void *fp, error_t **error, int32_t signature_length)
{
    if (signature_length != NS(gzseek)((gzFile) fp, signature_length, SEEK_SET)) {
        int errnum;
        const char *zerrstr;

        zerrstr = NS(gzerror)((gzFile) fp, &errnum);
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

    if (-1 == (ret = NS(gzread)((gzFile) fp, buffer, max_len))) {
        int errnum;
        const char *zerrstr;

        zerrstr = NS(gzerror)((gzFile) fp, &errnum);
        if (Z_ERRNO == errnum) {
            error_set(error, WARN, "zlib external error from gzread(): %s", strerror(errno));
        } else {
            error_set(error, WARN, "zlib internal error from gzread(): %s", zerrstr);
        }
    }
// debug("asked = %d, get = %d", max_len, ret);

    return ret;
}

reader_imp_t zlib_reader_imp =
{
    FALSE,
    "gzip",
#ifdef DYNAMIC_READERS
    zlib_available, // TODO: call DL_UNLOAD at the end
#endif /* DYNAMIC_READERS */
    zlib_dopen,
    zlib_close,
    zlib_eof,
    zlib_readBytes
#ifndef NO_PHYSICAL_REWIND
    , zlib_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
