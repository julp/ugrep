#include <unistd.h>
#include <errno.h>

#include "common.h"

static void *stdio_dopen(error_t **error, int fd, const char * const filename)
{
    FILE *fp;

    errno = 0;
    if (NULL == (fp = fdopen(fd, "r"))) {
        error_set(error, WARN, "fdopen failed on %s: %s", filename, strerror(errno));
    }

    return fp;
}

static void stdio_close(void *fp)
{
    if (STDIN_FILENO != fileno((FILE *) fp)) {
        fclose((FILE *) fp);
    }
}

static UBool stdio_eof(void *fp)
{
    return feof((FILE *) fp);
}

#ifndef NO_PHYSICAL_REWIND
static UBool stdio_rewindTo(void *fp, error_t **error, int32_t signature_length)
{
    if (0 != fseek((FILE *) fp, (long) signature_length, SEEK_SET)) {
        error_set(error, WARN, "fseek failed: %s", strerror(errno));
        return FALSE;
    }

    return TRUE;
}
#endif /* !NO_PHYSICAL_REWIND */

static int32_t stdio_readBytes(void *fp, error_t **error, char *buffer, size_t max_len)
{
    int ret;

    errno = 0;
    if (-1 == (ret = fread(buffer, sizeof(*buffer), max_len, (FILE *) fp))) {
        error_set(error, WARN, "fread failed: %s", strerror(errno));
    }

    return ret;
}

reader_imp_t stdio_reader_imp =
{
    FALSE,
    "stdio",
#ifdef DYNAMIC_READERS
    NULL,
#endif /* DYNAMIC_READERS */
    stdio_dopen,
    stdio_close,
    stdio_eof,
    stdio_readBytes
#ifndef NO_PHYSICAL_REWIND
    , stdio_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
