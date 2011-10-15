#ifndef READER_H

# define READER_H

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

typedef struct {
    UBool internal;
    const char *name;
    void *(*dopen)(error_t **, int, const char * const);
    void (*close)(void *);
    UBool (*eof)(void *);
    int32_t (*readBytes)(void *, error_t **, char *, size_t);
    void (*rewindTo)(void *, int32_t);
} reader_imp_t;

# ifdef DEBUG
#  define CHAR_BUFFER_SIZE  8
#  define UCHAR_BUFFER_SIZE 8
# else
#  define CHAR_BUFFER_SIZE  BUFSIZ
#  define UCHAR_BUFFER_SIZE BUFSIZ
#endif /* DEBUG */

typedef struct {
    const char *encoding;
    const char *sourcename;
    const char *default_encoding;
    const reader_imp_t *imp;
    const reader_imp_t *default_imp;
    void *priv_user;
    int binbehave;
    int32_t signature_length;
    size_t size;
    size_t lineno;
    UBool binary;

    int fd;   /* responsability of imp to close it if necessary */
    void *fp; /* responsability of imp to free and/or close it if necessary */
    UConverter *ucnv;
    struct {
        char buffer[CHAR_BUFFER_SIZE]; /* /!\ usage restricted to fill_buffer /!\ */
        char *ptr;                     /* /!\ usage restricted to fill_buffer /!\ */
        char *end;                     /* /!\ usage restricted to fill_buffer /!\ */
        const char *limit;             /* /!\ usage restricted to fill_buffer /!\ */
    } byte;
    struct {
        UChar buffer[UCHAR_BUFFER_SIZE];
        UChar *ptr;
        UChar *internalEnd; /* /!\ usage restricted to fill_buffer /!\ */
        UChar *externalEnd;
        const UChar *limit;
    } utf16;
} reader_t;

#define DEFAULT_READER_NAME "mmap"

void reader_close(reader_t *) NONNULL(1);
UBool reader_eof(reader_t *) NONNULL(1);
const reader_imp_t *reader_get_by_name(const char *);
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

#endif /* !READER_H */
