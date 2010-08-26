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
    size_t signature_length;
} stdiofd_t;

static void *stdiofd_open(const char *filename, int fd)
{
    stdiofd_t *stdiofd;

    stdiofd = mem_new(*stdiofd);
    //if (NULL == (stdiofd->fp = fopen(filename, "r"))) {
    //if (NULL == (stdiofd->ufp = u_fopen(filename, "r", NULL, NULL))) {
    if (NULL == (stdiofd->fp = fdopen(fd, "r"))) {
        msg("can't open %s: %s", filename, strerror(errno));
        goto failed;
    }
    if (NULL == (stdiofd->ufp = u_fadopt(stdiofd->fp, NULL, NULL))) {
        goto failed;
    }
    stdiofd->signature_length = 0;
    //stdiofd->ufp = NULL;
    stdiofd->fp = u_fgetfile(stdiofd->ufp);

    return stdiofd;

failed:
    free(stdiofd);
    return NULL;
}

static void stdiofd_close(void *data)
{
    FETCH_DATA(data, stdiofd, stdiofd_t);

    u_fclose(stdiofd->ufp);
}

static size_t stdiofd_readuchars(void *data, UChar32 *buffer, size_t max_len)
{
    size_t i;
    UChar32 c;
    FETCH_DATA(data, stdiofd, stdiofd_t);

    //return u_file_read(buffer, max_len, stdiofd->ufp);
    for (i = 0; U_EOF != (c = u_fgetcx(stdiofd->ufp)) && i < max_len; i++) {
        buffer[i] = c;
    }
    //buffer[i + 1] = U_NUL;

    return i;
}

static void stdiofd_rewind(void *data)
{
    FETCH_DATA(data, stdiofd, stdiofd_t);

    fseek(stdiofd->fp, (long) stdiofd->signature_length, SEEK_SET);
}

static UBool stdiofd_readline(void *data, UString *ustr)
{
    UChar c;
    FETCH_DATA(data, stdiofd, stdiofd_t);

    while (U_EOF != (c = u_fgetc(stdiofd->ufp)) && U_LF != c) {
        ustring_append_char(ustr, c);
    }

    return TRUE;
}

static size_t stdiofd_readbytes(void *data, char *buffer, size_t max_len)
{
    /*int fd;
    struct stat st;*/
    FETCH_DATA(data, stdiofd, stdiofd_t);

    /*if (-1 == (fd = fileno(stdiofd->fp))) {
        msg("fileno %s: %s", "TODO: filename", strerror(errno));
        return 0;
    }
    if (-1 == (fstat(fd, &st))) {
        msg("can't stat %s: %s", "TODO: filename", strerror(errno));
        return 0;
    }*/
    return fread(buffer, sizeof(*buffer), max_len/*st.st_size < max_len ? st.st_size : max_len*/, stdiofd->fp);
}

static void/*UBool*/ stdiofd_set_encoding(void *data, const char *encoding)
{
    FETCH_DATA(data, stdiofd, stdiofd_t);

    //stdiofd->ufp = u_fadopt(stdiofd->fp, NULL, encoding);
    u_fsetcodepage(encoding, stdiofd->ufp);
}

static void stdiofd_set_signature_length(void *data, size_t signature_length)
{
    FETCH_DATA(data, stdiofd, stdiofd_t);

    stdiofd->signature_length = signature_length;
}

static UBool stdiofd_eof(void *data)
{
    FETCH_DATA(data, stdiofd, stdiofd_t);

    return u_feof(stdiofd->ufp);
}

static UBool stdiofd_seekable(void *data)
{
    FETCH_DATA(data, stdiofd, stdiofd_t);

    return STDIN_FILENO != fileno(stdiofd->fp);
}

reader_t stdio_reader =
{
    "stdio",
    stdiofd_open,
    stdiofd_close,
    stdiofd_eof,
    stdiofd_seekable,
    stdiofd_readline,
    stdiofd_readbytes,
    stdiofd_readuchars,
    stdiofd_set_signature_length,
    stdiofd_set_encoding,
    stdiofd_rewind
};
