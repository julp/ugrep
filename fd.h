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
    int32_t signature_length;
    size_t filesize;
    size_t lineno;
    size_t matches; // TODO: make it as a private field (void *)
    UBool binary;
} fd_t;

extern int binbehave;

extern const fd_t NULL_FD;

void fd_close(fd_t *);
UBool fd_eof(fd_t *);
UBool fd_open(error_t **, fd_t *, const char *);
UBool fd_readline(error_t **, fd_t *, UString *);

#endif /* FD_H */
