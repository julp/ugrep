#include <bzlib.h>
#include <errno.h>

#include "common.h"

typedef struct {
    UBool eof;
    FILE *f;
    BZFILE *fp;
} BZIP2;

static void *bzip2_dopen(error_t **error, int fd, const char * const filename)
{
    BZIP2 *this;
    int bzerror;

    this = mem_new(*this);
    this->eof = FALSE;
    if (NULL == (this->f = fdopen(fd, "rb"))) {
        error_set(error, WARN, "fdopen failed on %s: %s", filename, strerror(errno));
        return NULL;
    }
    this->fp = BZ2_bzReadOpen(&bzerror, this->f, 0, 0, NULL, 0);
    if (BZ_OK != bzerror) {
        error_set(error, WARN, "bzip2 internal error from BZ2_bzReadOpen(): %s", BZ2_bzerror(this->fp, &bzerror));
        BZ2_bzReadClose(&bzerror, this->fp);
        return NULL;
    }

    return this;
}

static void bzip2_close(void *fp)
{
    BZIP2 *this;
    int bzerror;

    this = (BZIP2 *) fp;
    BZ2_bzReadClose(&bzerror, this->fp);
    fclose(this->f);
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
    BZ2_bzReadClose(&bzerror, this->fp);
    this->eof = FALSE;
    if (0 != fseek(this->f, (long) signature_length, SEEK_SET)) {
        error_set(error, WARN, "fseek failed: %s", strerror(errno));
        return FALSE;
    }
    this->fp = BZ2_bzReadOpen(&bzerror, this->f, 0, 0, NULL, 0);
    if (BZ_OK != bzerror) {
        error_set(error, WARN, "bzip2 internal error from BZ2_bzReadOpen(): %s", BZ2_bzerror(this->fp, &bzerror));
        BZ2_bzReadClose(&bzerror, this->fp);
        return FALSE;
    }
    if (signature_length > 0) {
        BZ2_bzRead(&bzerror, this->fp, buffer, signature_length);
        switch (bzerror) {
            case BZ_OK:
                break;
            case BZ_STREAM_END:
                this->eof = TRUE;
                break;
            default:
                error_set(error, WARN, "bzip2 internal error from BZ2_bzread(): %s", BZ2_bzerror(this->fp, &bzerror));
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
    ret = BZ2_bzRead(&bzerror, this->fp, buffer, max_len);
    switch (bzerror) {
        case BZ_OK:
            break;
        case BZ_STREAM_END:
            this->eof = TRUE;
            break;
        default:
            error_set(error, WARN, "bzip2 internal error from BZ2_bzread(): %s", BZ2_bzerror(this->fp, &bzerror));
            return -1;
    }

    return ret;
}

reader_imp_t bzip2_reader_imp =
{
    FALSE,
    "bzip2",
    bzip2_dopen,
    bzip2_close,
    bzip2_eof,
    bzip2_readBytes
#ifndef NO_PHYSICAL_REWIND
    , bzip2_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
