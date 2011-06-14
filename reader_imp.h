#ifndef READER_H

# define READER_H

typedef struct {
    UBool internal;
    const char *name;
    void *(*open)(error_t **, const char *, int);
    void (*close)(void *);
    UBool (*eof)(void *);
    UBool (*seekable)(void *);
    UBool (*readline)(error_t **error, void *, UString *);
    size_t (*readbytes)(void *, char *, size_t);                         /* Caller must append trailing \0 */
    int32_t (*readuchars)(error_t **error, void *, UChar *, size_t);     /* Caller must append trailing \0 */
    int32_t (*readuchars32)(error_t **error, void *, UChar32 *, size_t); /* Caller must append trailing \0 */
    UBool (*has_encoding)(void *);
    const char *(*get_encoding)(error_t **error, void *);
    UBool (*set_encoding)(error_t **, void *, const char *);
    void (*rewind)(void *, int32_t); /* Caller must provide BOM length (as 2nd argument) */
} reader_imp_t;

# define STRING_READUCHARS(error, ucnv, ptr, end, buffer, max_len)                          \
    do {                                                                                    \
        UErrorCode status;                                                                  \
        UChar *dest;                                                                        \
        const UChar *uend;                                                                  \
                                                                                            \
        status = U_ZERO_ERROR;                                                              \
        dest = buffer;                                                                      \
        uend = buffer + max_len;                                                            \
        ucnv_toUnicode(ucnv, &dest, uend, (const char **) &ptr, end, NULL, FALSE, &status); \
        if (U_FAILURE(status)) {                                                            \
            icu_error_set(error, FATAL, status, "ucnv_toUnicode");                          \
            return -1;                                                                      \
        }                                                                                   \
                                                                                            \
        return dest - buffer;                                                               \
    } while (0);

# define STRING_READUCHARS32(error, ucnv, ptr, end, buffer, max_len)         \
    do {                                                                     \
        UChar32 c;                                                           \
        int32_t i;                                                           \
        UErrorCode status;                                                   \
                                                                             \
        status = U_ZERO_ERROR;                                               \
        for (i = 0; i < max_len && ptr < end; i++) {                         \
            c = ucnv_getNextUChar(ucnv, (const char **) &ptr, end, &status); \
            if (U_FAILURE(status)) {                                         \
                icu_error_set(error, FATAL, status, "ucnv_getNextUChar");    \
                return -1;                                                   \
            }                                                                \
            buffer[i] = c;                                                   \
        }                                                                    \
                                                                             \
        return i;                                                            \
    } while (0);

# define STRING_GET_ENCODING(error, ucnv)                        \
    do {                                                         \
        UErrorCode status;                                       \
        const char *encoding;                                    \
                                                                 \
        status = U_ZERO_ERROR;                                   \
        encoding = ucnv_getName(ucnv, &status);                  \
        if (U_SUCCESS(status)) {                                 \
            return encoding;                                     \
        } else {                                                 \
            icu_error_set(error, FATAL, status, "ucnv_getName"); \
            return NULL;                                         \
        }                                                        \
    } while (0);

#endif /* READER_H */
