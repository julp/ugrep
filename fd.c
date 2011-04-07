#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unicode/uchar.h>
#include <unicode/ucsdet.h>

#include "ugrep.h"
#include "reader_decl.h"

#define MIN_CONFIDENCE  39   // Minimum confidence for a match (in percents)
#define MAX_ENC_REL_LEN 4096 // Maximum relevant length for encoding analyse (in bytes)
#define MAX_BIN_REL_LEN 4096 // Maximum relevant length for binary analyse

int binbehave = BIN_FILE_SKIP;

static UBool stdin_is_tty(void) {
    return (1 == isatty(STDIN_FILENO));
}

static UBool is_binary_uchar(UChar32 c)
{
    return !u_isprint(c) && !u_isspace(c) && U_BS != c;
}

static UBool is_binary(UChar32 *buffer, size_t buffer_len)
{
    UChar32 *p;

    for (p = buffer; U_NUL != *p; p++) {
        if (is_binary_uchar(*p)) {
            return TRUE;
        }
    }

    return ((size_t)(p - buffer)) < buffer_len;
}

void fd_close(fd_t *fd)
{
    fd->reader->close(fd->reader_data);
    free(fd->reader_data);
}

UBool fd_open(error_t **error, fd_t *fd, const char *filename)
{
    int fsfd;
    /*struct stat st;*/
    UErrorCode status;
    size_t buffer_len;
    const char *encoding;
    int32_t signature_length;
    char buffer[MAX_ENC_REL_LEN + 1];

    /*if (-1 == (stat(filename, &st))) {
        error_set(error, WARN, "can't stat %s: %s", filename, strerror(errno));
        return FALSE;
    }*/

    fd->reader_data = NULL;

    if (!strcmp("-", filename)) {
        if (!stdin_is_tty()) {
            error_set(error, WARN, "Sorry, can't work with redirected or piped stdin (not seekable, sources can use many codepage). Skip stdin.");
            goto failed;
        }
        fd->filename = "(standard input)";
        fd->reader = &stdio_reader;
        fsfd = STDIN_FILENO;
    } else {
        fd->filename = filename;
        if (-1 == (fsfd = open(filename, O_RDONLY))) {
            error_set(error, WARN, "can't open %s: %s", filename, strerror(errno));
            goto failed;
        }
    }

    if (NULL == (fd->reader_data = fd->reader->open(error, filename, fsfd))) {
        goto failed;
    }

    encoding = NULL;
    signature_length = 0;
    status = U_ZERO_ERROR;
    fd->lineno = 0;
    /*fd->filesize = st.st_size;*/
    fd->matches = 0;
    fd->binary = FALSE;

    if (fd->reader->seekable(fd->reader_data)) {
        if (0 != (buffer_len = fd->reader->readbytes(fd->reader_data, buffer, MAX_ENC_REL_LEN))) {
            buffer[buffer_len] = '\0';
            encoding = ucnv_detectUnicodeSignature(buffer, buffer_len, &signature_length, &status);
            if (U_SUCCESS(status)) {
                if (NULL == encoding) {
                    int32_t confidence;
                    UCharsetDetector *csd;
                    const char *tmpencoding;
                    const UCharsetMatch *ucm;

                    csd = ucsdet_open(&status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_open");
                        goto failed;
                    }
                    ucsdet_setText(csd, buffer, buffer_len, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_setText");
                        goto failed;
                    }
                    ucm = ucsdet_detect(csd, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_detect");
                        goto failed;
                    }
                    confidence = ucsdet_getConfidence(ucm, &status);
                    tmpencoding = ucsdet_getName(ucm, &status);
                    if (U_FAILURE(status)) {
                        icu_error_set(error, WARN, status, "ucsdet_getName");
                        ucsdet_close(csd);
                        goto failed;
                    }
                    if (confidence > MIN_CONFIDENCE) {
                        encoding = tmpencoding;
                        //debug("%s, confidence of " GREEN("%d%%") " for " YELLOW("%s"), filename, confidence, tmpencoding);
                    } else {
                        //debug("%s, confidence of " RED("%d%%") " for " YELLOW("%s"), filename, confidence, tmpencoding);
                        //encoding = "US-ASCII";
                    }
                    ucsdet_close(csd);
                } else {
                    fd->reader->set_signature_length(fd->reader_data, signature_length);
                }
                debug("%s, file encoding = %s", filename, encoding);
                fd->encoding = encoding;
                if (!fd->reader->set_encoding(error, fd->reader_data, encoding)) {
                    goto failed;
                }
                fd->reader->rewind(fd->reader_data);
                if (BIN_FILE_TEXT != binbehave) {
                    int32_t ubuffer_len;
                    UChar32 ubuffer[MAX_BIN_REL_LEN + 1];

                    if (-1 == (ubuffer_len = fd->reader->readuchars(error, fd->reader_data, ubuffer, MAX_BIN_REL_LEN))) {
                        goto failed;
                    }
                    ubuffer[ubuffer_len] = U_NUL;
                    fd->binary = is_binary(ubuffer, ubuffer_len);
                    debug("%s, binary file : %s", filename, fd->binary ? RED("yes") : GREEN("no"));
                    if (fd->binary) {
                        if (BIN_FILE_SKIP == binbehave) {
                            goto failed;
                        }
                    }
                    fd->reader->rewind(fd->reader_data);
                }
            } else {
                icu_error_set(error, WARN, status, "ucnv_detectUnicodeSignature");
                goto failed;
            }
        }
    }

    return TRUE;
failed:
    if (NULL != fd->reader_data) {
        fd_close(fd);
    }
    return FALSE;
}

UBool fd_eof(fd_t *fd) {
    return fd->reader->eof(fd->reader_data);
}

UBool fd_readline(error_t **error, fd_t *fd, UString *ustr)
{
    ustring_truncate(ustr);
    return fd->reader->readline(error, fd->reader_data, ustr);
}
