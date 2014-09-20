#include <sys/types.h>
#include <unistd.h>
#include <lzma.h>
#include <errno.h>

#include "common.h"

typedef struct {
    int fd;
    UBool eof;
    lzma_stream strm;
} LZMA;

static const char *lzma_strerror(lzma_ret ret)
{
    switch (ret) {
        case LZMA_OK:
            return "operation completed successfully";
        case LZMA_STREAM_END:
            return "end of stream was reached";
        case LZMA_NO_CHECK:
            return "input stream has no integrity check";
        case LZMA_UNSUPPORTED_CHECK:
            return "cannot calculate the integrity check";
        case LZMA_GET_CHECK:
            return "integrity check type is now available";
        case LZMA_MEM_ERROR:
            return "cannot allocate memory";
        case LZMA_MEMLIMIT_ERROR:
            return "memory usage limit was reached";
        case LZMA_FORMAT_ERROR:
            return "file format not recognized";
        case LZMA_OPTIONS_ERROR:
            return "invalid or unsupported options";
        case LZMA_DATA_ERROR:
            return "data is corrupt";
        case LZMA_BUF_ERROR:
            return "no progress is possible (stream is truncated or corrupt)";
        case LZMA_PROG_ERROR:
            return "programming error";
        default:
            return "unindentified liblzma error";
    }
}

#ifdef DYNAMIC_READERS
static const char *(*DRNS(lzma_stream_decoder))(lzma_stream *, uint64_t, uint32_t) = NULL;
static lzma_ret (*DRNS(lzma_code))(lzma_stream *, lzma_action) = NULL;
static void (*DRNS(lzma_end))(lzma_stream *strm) = NULL;

static UBool lzma_trydload(void)
{
    DL_HANDLE handle;

    handle = DL_LOAD("lzma", 5);
    if (!handle) {
        if (HAVE_DL_ERROR) {
            debug("failed loading lzma: %s", DL_ERROR);
        } else {
            debug("failed loading lzma");
        }
        return FALSE;
    }
    DL_GET_SYM(handle, DRNS(lzma_stream_decoder), "lzma_stream_decoder");
    DL_GET_SYM(handle, DRNS(lzma_code), "lzma_code");
    DL_GET_SYM(handle, DRNS(lzma_end), "lzma_end");
    env_register_resource(handle, (func_dtor_t) DL_UNLOAD);

    return TRUE;
}
#endif /* DYNAMIC_READERS */

static void *lzma_dopen(error_t **error, int fd, const char * const UNUSED(filename))
{
    LZMA *this;
    lzma_ret r;
    lzma_stream null_strm = LZMA_STREAM_INIT;

    this = mem_new(*this);
    this->fd = fd;
    this->eof = FALSE;
    this->strm = null_strm;
    if (LZMA_OK != (r = DRNS(lzma_stream_decoder)(&this->strm, UINT64_MAX, 0))) {
        error_set(error, WARN, "lzma internal error from lzma_stream_decoder()", lzma_strerror(r));
        free(this);
        return NULL;
    }

    return this;
}

static void lzma_close(void *fp)
{
    LZMA *this;

    this = (LZMA *) fp;
    DRNS(lzma_end)(&this->strm);
    // NOTE: we have not to close this->fd
    free(this);
}

static UBool lzma_eof(void *fp)
{
    LZMA *this;

    this = (LZMA *) fp;

    return this->eof;
}

#ifndef NO_PHYSICAL_REWIND
# define SIG_MAX_LEN 5
static UBool lzma_rewindTo(void *fp, error_t **error, int32_t signature_length)
{
    LZMA *this;

    this = (LZMA *) fp;
    if (((off_t) -1) == lseek(this->fd, (off_t) signature_length, SEEK_SET)) {
        error_set(error, WARN, "lseek() failed: %s", strerror(errno));
        return FALSE;
    }
    this->eof = FALSE;

    return TRUE;
}
#endif /* !NO_PHYSICAL_REWIND */

static int32_t lzma_readBytes(void *fp, error_t **error, char *buffer, size_t max_len)
{
    LZMA *this;
    lzma_ret r;
    int32_t ret;
    lzma_action action;
    uint8_t in_buf[CHAR_BUFFER_SIZE];

    action = LZMA_RUN;
    this = (LZMA *) fp;
    if (this->eof) {
        return 0;
    }
    this->strm.next_out = (uint8_t *) buffer;
    this->strm.avail_out = max_len;
    this->strm.next_in = in_buf;
    if (0 == (ret = read(this->fd, in_buf, ARRAY_SIZE(in_buf)))) {
        action = LZMA_FINISH;
    } else if (ret < 0) {
        error_set(error, WARN, "lzma internal error from read()", strerror(errno));
        return -1;
    }
    this->strm.avail_in = ret;
    switch (r = DRNS(lzma_code)(&this->strm, action)) {
        case LZMA_STREAM_END:
            this->eof = TRUE;
            break;
        case LZMA_OK:
            break;
        default:
            error_set(error, WARN, "lzma internal error from lzma_code(): %s", lzma_strerror(r));
            return -1;
    }
//     debug("asked = %d, get = %d", max_len, max_len - this->strm.avail_out);

    return max_len - this->strm.avail_out;
}

reader_imp_t lzma_reader_imp =
{
    FALSE,
    "lzma",
#if 0
    "\xFD\x37\x7A\x58\x5A\x00",
#endif
#ifdef DYNAMIC_READERS
    lzma_trydload,
#endif /* DYNAMIC_READERS */
    lzma_dopen,
    lzma_close,
    lzma_eof,
    lzma_readBytes
#ifndef NO_PHYSICAL_REWIND
    , lzma_rewindTo
#endif /* !NO_PHYSICAL_REWIND */
};
