#ifndef FD_H

# define FD_H

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

#define MAX_NFC_FACTOR  3
#define MAX_NFD_FACTOR 18

#define UTF16_MAX_NFC_FACTOR (U16_MAX_LENGTH * MAX_NFC_FACTOR)
#define UTF16_MAX_NFD_FACTOR (U16_MAX_LENGTH * MAX_NFD_FACTOR)

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
    int32_t nfd_count;
    UChar nfd_buffer[UTF16_MAX_NFD_FACTOR];
} reader_t;

#define DEFAULT_READER_NAME "mmap"

void reader_close(reader_t *) NONNULL(1);
UBool reader_eof(reader_t *) NONNULL(1);
reader_imp_t *reader_get_by_name(const char *);
void *reader_get_user_data(reader_t *) NONNULL(1);
void reader_init(reader_t *, const char *) NONNULL(1);
UBool reader_open(reader_t *, error_t **, const char *) NONNULL(1, 3);
UBool reader_open_stdin(reader_t *, error_t **) NONNULL(1);
UBool reader_open_string(reader_t *, error_t **, const char *) NONNULL(1, 3);
UBool reader_readline(reader_t *, error_t **, UString *) NONNULL(1, 3);
int32_t reader_readuchars(reader_t *, error_t **, UChar *, size_t) NONNULL(1, 3);
void reader_set_binary_behavior(reader_t *, int) NONNULL(1);
void reader_set_default_encoding(reader_t *, const char *) NONNULL(1);
UBool reader_set_encoding(reader_t *, error_t **, const char *) NONNULL(1);
UBool reader_set_imp_by_name(reader_t *, const char *) NONNULL(1);
void reader_set_user_data(reader_t *, void *) NONNULL(1);

#endif /* FD_H */
