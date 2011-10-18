#include <zlib.h>
#include <errno.h>

#include "common.h"

static void *zlib_dopen(error_t **error, int fd, const char * const filename)
{
    gzFile *fp;

    if (NULL == (fp = gzdopen(fd, "rb"))) {
        error_set(error, WARN, "gzdopen failed on %s", filename);
    }

    return fp;
}

static void zlib_close(void *fp)
{
    gzclose_r((gzFile *) fp);
}

static UBool zlib_eof(void *fp)
{
// debug("eof = %d", gzeof((gzFile *) fp));
    return gzeof((gzFile *) fp);
}

#ifndef NO_PHYSICAL_REWIND
static UBool zlib_rewindTo(void *fp, error_t **error, int32_t signature_length)
{
    if (signature_length != gzseek((gzFile *) fp, signature_length, SEEK_SET)) {
        int errnum;
        const char *zerrstr;

        zerrstr = gzerror((gzFile *) fp, &errnum);
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

    if (-1 == (ret = gzread((gzFile *) fp, buffer, max_len))) {
        int errnum;
        const char *zerrstr;

        zerrstr = gzerror((gzFile *) fp, &errnum);
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
    zlib_dopen,
    zlib_close,
    zlib_eof,
    zlib_readBytes
#ifndef NO_PHYSICAL_REWIND
    , zlib_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
