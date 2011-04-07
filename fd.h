#ifndef FD_H

# define FD_H

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

typedef struct {
    const char *filename;
    const char *encoding;
    reader_t *reader;
    void *reader_data;
    size_t filesize;
    size_t lineno;
    size_t matches;
    UBool binary;
} fd_t;

extern int binbehave;

void fd_close(fd_t *);
UBool fd_eof(fd_t *);
UBool fd_open(error_t **, fd_t *, const char *);
UBool fd_readline(error_t **, fd_t *, UString *);

#endif /* FD_H */
