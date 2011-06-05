#include "ugrep.h"

#include <unistd.h>
#include <stdio.h>
#include <unicode/ustdio.h>

/*#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>*/

#include <errno.h>

typedef struct {
    FILE *fp;
    UFILE *ufp;
} stdiofd_t;

static void *stdio_open(error_t **error, const char *filename, int fd)
{
    stdiofd_t *this;

    this = mem_new(*this);
    //if (NULL == (this->fp = fopen(filename, "r"))) {
    //if (NULL == (this->ufp = u_fopen(filename, "r", NULL, NULL))) {
    if (NULL == (this->fp = fdopen(fd, "r"))) {
        error_set(error, WARN, "can't open %s: %s", filename, strerror(errno));
        goto failed;
    }
    if (fd == STDIN_FILENO) {
        if (NULL == (this->ufp = u_finit(this->fp, NULL, NULL))) {
            goto failed;
        }
    } else {
        if (NULL == (this->ufp = u_fadopt(this->fp, NULL, NULL))) {
            goto failed;
        }
    }
    //this->ufp = NULL;
    this->fp = u_fgetfile(this->ufp);

    return this;

failed:
    free(this);
    return NULL;
}

static void stdio_close(void *data)
{
    FETCH_DATA(data, this, stdiofd_t);

    u_fclose(this->ufp);
}

static int32_t stdio_readuchars32(error_t **UNUSED(error), void *data, UChar32 *buffer, size_t max_len)
{
    size_t i;
    UChar32 c;
    FETCH_DATA(data, this, stdiofd_t);

    //return u_file_read(buffer, max_len, this->ufp);
    for (i = 0; U_EOF != (c = u_fgetcx(this->ufp)) && i < max_len; i++) {
        buffer[i] = c;
    }

    return i;
}

static void stdio_rewind(void *data, int32_t signature_length)
{
    FETCH_DATA(data, this, stdiofd_t);

    fseek(this->fp, signature_length, SEEK_SET);
}

static UBool stdio_readline(error_t **UNUSED(error), void *data, UString *ustr)
{
    UChar c;
    FETCH_DATA(data, this, stdiofd_t);

    while (U_EOF != (c = u_fgetc(this->ufp)) && U_LF != c) {
        ustring_append_char(ustr, c);
    }

    return TRUE;
}

static size_t stdio_readbytes(void *data, char *buffer, size_t max_len)
{
    /*int fd;
    struct stat st;*/
    FETCH_DATA(data, this, stdiofd_t);

    /*if (-1 == (fd = fileno(this->fp))) {
        fprintf(stderr, "fileno %s: %s\n", "TODO: filename", strerror(errno));
        return 0;
    }
    if (-1 == (fstat(fd, &st))) {
        fprintf(stderr, "can't stat %s: %s\n", "TODO: filename", strerror(errno));
        return 0;
    }*/
    return fread(buffer, sizeof(*buffer), max_len/*st.st_size < max_len ? st.st_size : max_len*/, this->fp);
}

static UBool stdio_has_encoding(void *UNUSED(data))
{
    return TRUE; // Each UFILE has it's own converter which is system default codepage by default
}

static const char *stdio_get_encoding(void *data)
{
    UErrorCode status;
    const char *encoding;
    FETCH_DATA(data, this, stdiofd_t);

    status = U_ZERO_ERROR;
    encoding = ucnv_getName(u_fgetConverter(this->ufp), &status);
    if (U_SUCCESS(status)) {
        return encoding;
    } else {
        //icu_error_set(error, FATAL, status, "ucnv_getName");
        return "ucnv_getName() failed";
    }
}

static UBool stdio_set_encoding(error_t **UNUSED(error), void *data, const char *encoding)
{
    FETCH_DATA(data, this, stdiofd_t);

    //this->ufp = u_fadopt(this->fp, NULL, encoding);
    return (0 == u_fsetcodepage(encoding, this->ufp));
}

static UBool stdio_eof(void *data)
{
    FETCH_DATA(data, this, stdiofd_t);

    return u_feof(this->ufp);
}

static UBool stdio_seekable(void *data)
{
    FETCH_DATA(data, this, stdiofd_t);

    return STDIN_FILENO != fileno(this->fp);
}

reader_t stdio_reader =
{
    "stdio",
    stdio_open,
    stdio_close,
    stdio_eof,
    stdio_seekable,
    stdio_readline,
    stdio_readbytes,
    stdio_readuchars32,
    stdio_has_encoding,
    stdio_get_encoding,
    stdio_set_encoding,
    stdio_rewind
};
