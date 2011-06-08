#ifndef FD_H

# define FD_H

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

typedef struct {
    const char *sourcename;
    const char *default_encoding;
    reader_imp_t *imp;
    reader_imp_t *default_imp;
    void *priv_imp;
    void *priv_user;
    int binbehave;
    int32_t signature_length;
    size_t size;
    size_t lineno;
    UBool binary;
} reader_t;

void reader_close(reader_t *);
UBool reader_eof(reader_t *);
reader_imp_t *reader_get_by_name(const char *);
void reader_init(reader_t *, const char *);
UBool reader_open(reader_t *, error_t **, const char *);
UBool reader_readline(reader_t *, error_t **, UString *);
void reader_set_binary_behavior(reader_t *, int);
void reader_set_default_encoding(reader_t *, const char *);
UBool reader_set_encoding(reader_t *, error_t **, const char *);
UBool reader_set_imp_by_name(reader_t *, const char *);

#endif /* FD_H */
